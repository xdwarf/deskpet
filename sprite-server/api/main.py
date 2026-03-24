"""
main.py — DeskPet sprite server API.

Routes:
  GET  /                              → serve the web UI (index.html)
  GET  /manifest                      → return current manifest.json as JSON
  GET  /published                     → list all published characters and their expressions
  POST /convert                       → upload GIF/PNG, returns preview info + frame count
  POST /publish                       → upload GIF/PNG, convert, save .sprite, bump manifest
  GET  /download/{character}/{expression} → download a published .sprite file
  POST /convert-mp4                   → upload MP4, returns a 240x240 GIF download
  POST /convert-mp4-batch             → upload multiple MP4s, returns a ZIP of GIFs
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
STATIC_DIR  = Path(__file__).parent / "static"

app = FastAPI(title="DeskPet Sprite Server", version="1.0.0")

# Serve static UI files at /static/*
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")


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
    """Return a dict of {character: [expression, ...]} for all .sprite files."""
    result: dict[str, list[str]] = {}
    if not SPRITES_DIR.exists():
        return result

    for char_dir in sorted(SPRITES_DIR.iterdir()):
        if not char_dir.is_dir():
            continue
        expressions = sorted(
            p.stem for p in char_dir.glob("*.sprite")
        )
        if expressions:
            result[char_dir.name] = expressions

    return result


# ---------------------------------------------------------------------------
# Convert (preview only — does not write to disk)
# ---------------------------------------------------------------------------

@app.post("/convert")
async def convert_preview(file: UploadFile = File(...)):
    """
    Upload an image, return info about how it would be converted.
    Does not write anything to /sprites.
    """
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
# Publish (convert + save + bump manifest)
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
        raise HTTPException(
            status_code=404,
            detail=f"{character}/{expression}.sprite not found",
        )
    return FileResponse(
        str(path),
        media_type="application/octet-stream",
        filename=f"{expression}.sprite",
    )


# ---------------------------------------------------------------------------
# Shared MP4 → GIF helper
# ---------------------------------------------------------------------------

def _mp4_to_gif(data: bytes, filename: str, fps: int) -> bytes:
    """Convert raw MP4 bytes to 240x240 GIF bytes using FFmpeg."""
    with tempfile.TemporaryDirectory() as tmp:
        mp4_path     = Path(tmp) / "input.mp4"
        palette_path = Path(tmp) / "palette.png"
        gif_path     = Path(tmp) / "output.gif"

        mp4_path.write_bytes(data)

        # Step 1 — generate optimised palette
        palette_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-vf", f"fps={fps},scale=240:240:flags=lanczos,palettegen=stats_mode=diff",
            str(palette_path)
        ]
        result = subprocess.run(palette_cmd, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError("Palette generation failed for " + filename + ": " + result.stderr.decode())

        # Step 2 — render GIF using palette
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
# Convert single MP4 → GIF
# ---------------------------------------------------------------------------

@app.post("/convert-mp4")
async def convert_mp4(
    file: UploadFile = File(...),
    fps: int = Form(12),
):
    if fps not in (8, 12, 18, 24):
        raise HTTPException(status_code=422, detail="fps must be 8, 12, 18, or 24")
    if not file.filename.lower().endswith(".mp4"):
        raise HTTPException(status_code=422, detail="Only MP4 files are accepted")

    data = await file.read()
    try:
        gif_bytes = _mp4_to_gif(data, file.filename, fps)
    except RuntimeError as e:
        raise HTTPException(status_code=500, detail=str(e))

    with tempfile.NamedTemporaryFile(delete=False, suffix=".gif") as out:
        out.write(gif_bytes)
        out_path = out.name

    original_name = Path(file.filename).stem
    return FileResponse(
        out_path,
        media_type="image/gif",
        filename=f"{original_name}_{fps}fps_240x240.gif",
    )


# ---------------------------------------------------------------------------
# Convert batch MP4s → ZIP of GIFs
# ---------------------------------------------------------------------------

@app.post("/convert-mp4-batch")
async def convert_mp4_batch(
    files: List[UploadFile] = File(...),
    fps: int = Form(12),
):
    if fps not in (8, 12, 18, 24):
        raise HTTPException(status_code=422, detail="fps must be 8, 12, 18, or 24")

    for f in files:
        if not f.filename.lower().endswith(".mp4"):
            raise HTTPException(status_code=422, detail=f"{f.filename} is not an MP4")

    # Build ZIP in memory
    zip_buffer = io.BytesIO()
    errors = []

    with zipfile.ZipFile(zip_buffer, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in files:
            data = await f.read()
            try:
                gif_bytes = _mp4_to_gif(data, f.filename, fps)
                gif_name  = Path(f.filename).stem + f"_{fps}fps_240x240.gif"
                zf.writestr(gif_name, gif_bytes)
            except RuntimeError as e:
                errors.append(str(e))

    if errors and len(errors) == len(files):
        raise HTTPException(status_code=500, detail="All conversions failed: " + "; ".join(errors))

    zip_buffer.seek(0)
    return StreamingResponse(
        zip_buffer,
        media_type="application/zip",
        headers={"Content-Disposition": f"attachment; filename=muni_gifs_{fps}fps.zip"}
    )