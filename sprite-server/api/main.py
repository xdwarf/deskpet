"""
main.py — DeskPet sprite server API.

Routes:
  GET  /                              → serve the web UI (index.html)
  GET  /manifest                      → return current manifest.json as JSON
  GET  /published                     → list all published characters and their expressions
  POST /convert                       → upload GIF/PNG, returns preview info + frame count
  POST /publish                       → upload GIF/PNG, convert, save .sprite, bump manifest
  GET  /download/{character}/{expression} → download a published .sprite file
  POST /convert-mp4                   → upload MP4(s), converts and saves GIFs to library
  GET  /gifs                          → list all GIFs in the library
  GET  /gifs/{filename}               → serve a GIF for preview
  DELETE /gifs/{filename}             → delete a GIF from the library
  POST /publish-from-gif              → publish a saved GIF directly to sprites
"""

import io
import os
import subprocess
import tempfile
import zipfile
from pathlib import Path
from typing import List

from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

import converter
import manifest as manifest_mod

# ---------------------------------------------------------------------------
SPRITES_DIR = Path(os.environ.get("SPRITES_DIR", "/sprites"))
GIFS_DIR    = Path(os.environ.get("GIFS_DIR", "/gifs"))
STATIC_DIR  = Path(__file__).parent / "static"

app = FastAPI(title="DeskPet Sprite Server", version="1.0.0")

app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

GIFS_DIR.mkdir(parents=True, exist_ok=True)


# ---------------------------------------------------------------------------
# UI
# ---------------------------------------------------------------------------

@app.get("/", include_in_schema=False)
def index():
    return FileResponse(str(STATIC_DIR / "index.html"))


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

@app.get("/manifest")
def get_manifest():
    return manifest_mod.load()


# ---------------------------------------------------------------------------
# Published sprites
# ---------------------------------------------------------------------------

@app.get("/published")
def get_published():
    result: dict[str, list[str]] = {}
    if not SPRITES_DIR.exists():
        return result
    for char_dir in sorted(SPRITES_DIR.iterdir()):
        if not char_dir.is_dir():
            continue
        expressions = sorted(p.stem for p in char_dir.glob("*.sprite"))
        if expressions:
            result[char_dir.name] = expressions
    return result


# ---------------------------------------------------------------------------
# Convert (preview only)
# ---------------------------------------------------------------------------

@app.post("/convert")
async def convert_preview(file: UploadFile = File(...)):
    data = await file.read()
    try:
        sprite_bytes, frame_count = converter.convert_to_sprite(data)
    except Exception as e:
        raise HTTPException(status_code=422, detail=str(e))
    return {
        "filename": file.filename,
        "frames": frame_count,
        "sprite_bytes": len(sprite_bytes),
        "frame_bytes": 240 * 240 * 2,
    }


# ---------------------------------------------------------------------------
# Publish from upload
# ---------------------------------------------------------------------------

@app.post("/publish")
async def publish(
    file: UploadFile = File(...),
    character: str   = Form(...),
    expression: str  = Form(...),
):
    character  = character.strip().lower()
    expression = expression.strip().lower()
    if not character or not expression:
        raise HTTPException(status_code=422, detail="character and expression are required")

    data = await file.read()
    try:
        sprite_bytes, frame_count = converter.convert_to_sprite(data)
    except Exception as e:
        raise HTTPException(status_code=422, detail=str(e))

    out_dir  = SPRITES_DIR / character
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{expression}.sprite"
    out_path.write_bytes(sprite_bytes)

    updated_manifest = manifest_mod.publish(character, expression)

    return {
        "character":  character,
        "expression": expression,
        "frames":     frame_count,
        "sprite_bytes": len(sprite_bytes),
        "path":       str(out_path.relative_to(SPRITES_DIR)),
        "manifest_version": updated_manifest["version"],
    }


# ---------------------------------------------------------------------------
# Download a published sprite file
# ---------------------------------------------------------------------------

@app.get("/download/{character}/{expression}")
def download_sprite(character: str, expression: str):
    path = SPRITES_DIR / character / f"{expression}.sprite"
    if not path.exists():
        raise HTTPException(status_code=404, detail=f"{character}/{expression}.sprite not found")
    return FileResponse(
        str(path),
        media_type="application/octet-stream",
        filename=f"{expression}.sprite",
    )


# ---------------------------------------------------------------------------
# Shared MP4 → GIF helper
# ---------------------------------------------------------------------------

def _mp4_to_gif(data: bytes, filename: str, fps: int) -> bytes:
    with tempfile.TemporaryDirectory() as tmp:
        mp4_path     = Path(tmp) / "input.mp4"
        palette_path = Path(tmp) / "palette.png"
        gif_path     = Path(tmp) / "output.gif"

        mp4_path.write_bytes(data)

        palette_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-vf", f"fps={fps},scale=240:240:flags=lanczos,palettegen=stats_mode=diff",
            str(palette_path)
        ]
        result = subprocess.run(palette_cmd, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError("Palette generation failed for " + filename + ": " + result.stderr.decode())

        gif_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-i", str(palette_path),
            "-lavfi", f"fps={fps},scale=240:240:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle",
            str(gif_path)
        ]
        result = subprocess.run(gif_cmd, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError("GIF conversion failed for " + filename + ": " + result.stderr.decode())

        return gif_path.read_bytes()

# ---------------------------------------------------------------------------
# Convert MP4 → transparent animated WebP (white background removed)
# Add this route anywhere after the _mp4_to_gif helper
# ---------------------------------------------------------------------------

@app.post("/convert-mp4-webp")
async def convert_mp4_webp(
    file: UploadFile = File(...),
    fps: int = Form(12),
    threshold: int = Form(240),
):
    """
    Convert a single MP4 to a transparent animated WebP.
    White (and near-white) pixels are replaced with transparency.
    threshold: 0-255, pixels where R,G,B are all >= this value become transparent.
    """
    if not file.filename.lower().endswith(".mp4"):
        raise HTTPException(status_code=422, detail="File must be an MP4")
    if fps not in (8, 12, 18, 24):
        raise HTTPException(status_code=422, detail="fps must be 8, 12, 18, or 24")

    data = await file.read()

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        mp4_path    = tmp / "input.mp4"
        frames_dir  = tmp / "frames"
        webp_path   = tmp / "output.webp"

        mp4_path.write_bytes(data)
        frames_dir.mkdir()

        # Extract frames as PNGs via ffmpeg
        extract_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-vf", f"fps={fps},scale=240:240:flags=lanczos",
            str(frames_dir / "frame%04d.png")
        ]
        result = subprocess.run(extract_cmd, capture_output=True)
        if result.returncode != 0:
            raise HTTPException(status_code=500, detail="Frame extraction failed: " + result.stderr.decode())

        frame_paths = sorted(frames_dir.glob("frame*.png"))
        if not frame_paths:
            raise HTTPException(status_code=500, detail="No frames extracted from MP4")

        # Strip white background from each frame
        from PIL import Image
        frames = []
        for fp in frame_paths:
            img = Image.open(fp).convert("RGBA")
            pixels = img.load()
            for y in range(img.height):
                for x in range(img.width):
                    r, g, b, a = pixels[x, y]
                    if r >= threshold and g >= threshold and b >= threshold:
                        pixels[x, y] = (r, g, b, 0)
            frames.append(img)

        # Save as animated WebP
        frames[0].save(
            str(webp_path),
            format="WEBP",
            save_all=True,
            append_images=frames[1:],
            duration=int(1000 / fps),
            loop=0,
            lossless=False,
            quality=90,
        )

        webp_bytes = webp_path.read_bytes()

    return StreamingResponse(
        io.BytesIO(webp_bytes),
        media_type="image/webp",
        headers={"Content-Disposition": f"attachment; filename=muni_transparent.webp"}
    )

# ---------------------------------------------------------------------------
# Convert MP4(s) → save to GIF library
# ---------------------------------------------------------------------------

@app.post("/convert-mp4")
async def convert_mp4(
    files: List[UploadFile] = File(...),
    fps: int = Form(12),
):
    if fps not in (8, 12, 18, 24):
        raise HTTPException(status_code=422, detail="fps must be 8, 12, 18, or 24")

    for f in files:
        if not f.filename.lower().endswith(".mp4"):
            raise HTTPException(status_code=422, detail=f"{f.filename} is not an MP4")

    saved = []
    errors = []

    for f in files:
        data = await f.read()
        try:
            gif_bytes = _mp4_to_gif(data, f.filename, fps)
            gif_name  = Path(f.filename).stem + f"_{fps}fps.gif"
            out_path  = GIFS_DIR / gif_name
            out_path.write_bytes(gif_bytes)
            saved.append(gif_name)
        except RuntimeError as e:
            errors.append(str(e))

    if errors and not saved:
        raise HTTPException(status_code=500, detail="All conversions failed: " + "; ".join(errors))

    return {"saved": saved, "errors": errors}


# ---------------------------------------------------------------------------
# GIF library — list
# ---------------------------------------------------------------------------

@app.get("/gifs")
def list_gifs():
    if not GIFS_DIR.exists():
        return []
    return sorted([f.name for f in GIFS_DIR.glob("*.gif")])


# ---------------------------------------------------------------------------
# GIF library — serve for preview
# ---------------------------------------------------------------------------

@app.get("/gifs/{filename}")
def serve_gif(filename: str):
    path = GIFS_DIR / filename
    if not path.exists():
        raise HTTPException(status_code=404, detail=f"{filename} not found")
    return FileResponse(str(path), media_type="image/gif")


# ---------------------------------------------------------------------------
# GIF library — delete
# ---------------------------------------------------------------------------

@app.delete("/gifs/{filename}")
def delete_gif(filename: str):
    path = GIFS_DIR / filename
    if not path.exists():
        raise HTTPException(status_code=404, detail=f"{filename} not found")
    path.unlink()
    return {"deleted": filename}


# ---------------------------------------------------------------------------
# Publish from GIF library
# ---------------------------------------------------------------------------

@app.post("/publish-from-gif")
async def publish_from_gif(
    filename:   str = Form(...),
    character:  str = Form(...),
    expression: str = Form(...),
):
    character  = character.strip().lower()
    expression = expression.strip().lower()
    if not character or not expression:
        raise HTTPException(status_code=422, detail="character and expression are required")

    gif_path = GIFS_DIR / filename
    if not gif_path.exists():
        raise HTTPException(status_code=404, detail=f"{filename} not found in GIF library")

    data = gif_path.read_bytes()
    try:
        sprite_bytes, frame_count = converter.convert_to_sprite(data)
    except Exception as e:
        raise HTTPException(status_code=422, detail=str(e))

    out_dir  = SPRITES_DIR / character
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{expression}.sprite"
    out_path.write_bytes(sprite_bytes)

    updated_manifest = manifest_mod.publish(character, expression)

    return {
        "character":  character,
        "expression": expression,
        "frames":     frame_count,
        "sprite_bytes": len(sprite_bytes),
        "path":       str(out_path.relative_to(SPRITES_DIR)),
        "manifest_version": updated_manifest["version"],
    }