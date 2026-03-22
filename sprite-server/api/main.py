"""
main.py — DeskPet sprite server API.

Routes:
  GET  /                  → serve the web UI (index.html)
  GET  /manifest          → return current manifest.json as JSON
  GET  /published         → list all published characters and their expressions
  POST /convert           → upload GIF/PNG, returns preview info + frame count
  POST /publish           → upload GIF/PNG, convert, save .sprite, bump manifest
"""

import os
import shutil
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
    """
    Convert and publish a sprite.
    Saves to /sprites/{character}/{expression}.sprite and bumps manifest version.
    """
    # Basic validation
    character  = character.strip().lower()
    expression = expression.strip().lower()
    if not character or not expression:
        raise HTTPException(status_code=422, detail="character and expression are required")

    data = await file.read()
    try:
        sprite_bytes, frame_count = converter.convert_to_sprite(data)
    except Exception as e:
        raise HTTPException(status_code=422, detail=str(e))

    # Write sprite file
    out_dir  = SPRITES_DIR / character
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{expression}.sprite"
    out_path.write_bytes(sprite_bytes)

    # Update manifest
    updated_manifest = manifest_mod.publish(character, expression)

    return {
        "character":  character,
        "expression": expression,
        "frames":     frame_count,
        "sprite_bytes": len(sprite_bytes),
        "path":       str(out_path.relative_to(SPRITES_DIR)),
        "manifest_version": updated_manifest["version"],
    }
