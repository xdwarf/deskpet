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
"""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi.responses import FileResponse, JSONResponse
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
# Convert MP4 → GIF
# ---------------------------------------------------------------------------

@app.post("/convert-mp4")
async def convert_mp4(
    file: UploadFile = File(...),
    fps: int = Form(12),
):
    """
    Upload an MP4, get back a 240x240 high-quality GIF.
    fps options: 8, 12, 24
    """
    if fps not in (8, 12, 24):
        raise HTTPException(status_code=422, detail="fps must be 8, 12, or 24")

    if not file.filename.lower().endswith(".mp4"):
        raise HTTPException(status_code=422, detail="Only MP4 files are accepted")

    data = await file.read()

    with tempfile.TemporaryDirectory() as tmp:
        mp4_path  = Path(tmp) / "input.mp4"
        palette_path = Path(tmp) / "palette.png"
        gif_path  = Path(tmp) / "output.gif"

        mp4_path.write_bytes(data)

        # Step 1 — generate optimised palette for best colour quality
        palette_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-vf", f"fps={fps},scale=240:240:flags=lanczos,palettegen=stats_mode=diff",
            str(palette_path)
        ]
        result = subprocess.run(palette_cmd, capture_output=True)
        if result.returncode != 0:
            raise HTTPException(status_code=500, detail="FFmpeg palette generation failed: " + result.stderr.decode())

        # Step 2 — render GIF using palette (high quality dithering)
        gif_cmd = [
            "ffmpeg", "-y",
            "-i", str(mp4_path),
            "-i", str(palette_path),
            "-lavfi", f"fps={fps},scale=240:240:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle",
            str(gif_path)
        ]
        result = subprocess.run(gif_cmd, capture_output=True)
        if result.returncode != 0:
            raise HTTPException(status_code=500, detail="FFmpeg GIF conversion failed: " + result.stderr.decode())

        gif_bytes = gif_path.read_bytes()

    # Return GIF as download
    with tempfile.NamedTemporaryFile(delete=False, suffix=".gif") as out:
        out.write(gif_bytes)
        out_path = out.name

    original_name = Path(file.filename).stem
    return FileResponse(
        out_path,
        media_type="image/gif",
        filename=f"{original_name}_{fps}fps_240x240.gif",
    )