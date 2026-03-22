"""
converter.py — GIF/PNG/JPEG → RGB565 .sprite file converter.

Each output frame is exactly 240×240×2 = 115,200 bytes of raw little-endian
uint16 RGB565 pixels, no header. The ESP32 reads frames sequentially.

Conversion steps per frame:
  1. Convert to RGBA so we have a consistent channel layout.
  2. Center-crop to a square (crop the longest axis to match the shortest).
  3. Resize to 240×240 with high-quality Lanczos resampling.
  4. Pack each pixel as: ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
     in little-endian byte order.
"""

import io
import struct
from PIL import Image, ImageSequence


DISPLAY_SIZE = 240


def _to_rgb565_frame(frame: Image.Image) -> bytes:
    """Convert a single PIL Image frame to 115,200 bytes of RGB565."""
    img = frame.convert("RGBA")

    # Center-crop to square
    w, h = img.size
    if w != h:
        side = min(w, h)
        left = (w - side) // 2
        top  = (h - side) // 2
        img = img.crop((left, top, left + side, top + side))

    # Resize to display resolution
    img = img.resize((DISPLAY_SIZE, DISPLAY_SIZE), Image.LANCZOS)

    # Pack pixels
    pixels = img.load()
    buf = bytearray(DISPLAY_SIZE * DISPLAY_SIZE * 2)
    idx = 0
    for y in range(DISPLAY_SIZE):
        for x in range(DISPLAY_SIZE):
            r, g, b, _ = pixels[x, y]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            struct.pack_into("<H", buf, idx, rgb565)
            idx += 2

    return bytes(buf)


def convert_to_sprite(data: bytes) -> tuple[bytes, int]:
    """
    Convert image data (GIF, PNG, JPEG, …) to a raw .sprite byte blob.

    Returns (sprite_bytes, frame_count).
    Each frame is DISPLAY_SIZE×DISPLAY_SIZE×2 bytes, frames are concatenated.
    """
    img = Image.open(io.BytesIO(data))

    frames: list[bytes] = []
    try:
        for frame in ImageSequence.Iterator(img):
            frames.append(_to_rgb565_frame(frame.copy()))
    except EOFError:
        pass

    if not frames:
        raise ValueError("Image contained no readable frames")

    return b"".join(frames), len(frames)
