/**
 * spriteParser.js
 * 
 * Parses .sprite binary files for the Muni Emulator.
 * 
 * .sprite format:
 *   - Raw binary, no header
 *   - Sequential RGB565 frames (little-endian, 16-bit per pixel)
 *   - Each frame: 240 × 240 × 2 = 115,200 bytes
 *   - Frame count derived from file size
 */

const FRAME_WIDTH = 240;
const FRAME_HEIGHT = 240;
const BYTES_PER_PIXEL = 2;
const FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIXEL; // 115,200 bytes

/**
 * Convert an RGB565 pixel (little-endian) to RGBA components.
 * 
 * RGB565 layout (16-bit):
 *   RRRR RGGG GGGB BBBB
 *   
 * @param {number} pixel - 16-bit RGB565 value
 * @returns {{ r: number, g: number, b: number, a: number }}
 */
function rgb565ToRGBA(pixel) {
  const r = ((pixel >> 11) & 0x1F) << 3;
  const g = ((pixel >> 5) & 0x3F) << 2;
  const b = (pixel & 0x1F) << 3;
  return { r, g, b, a: 255 };
}

/**
 * Parse a .sprite binary buffer into an array of ImageData frames.
 * 
 * @param {ArrayBuffer} buffer - Raw .sprite file data
 * @returns {ImageData[]} Array of 240×240 ImageData frames
 */
export function parseSpriteFile(buffer) {
  const totalBytes = buffer.byteLength;
  const frameCount = Math.floor(totalBytes / FRAME_SIZE);

  if (frameCount === 0) {
    console.warn(`[spriteParser] File too small for even one frame (${totalBytes} bytes)`);
    return [];
  }

  if (totalBytes % FRAME_SIZE !== 0) {
    console.warn(
      `[spriteParser] File size (${totalBytes}) not evenly divisible by frame size (${FRAME_SIZE}). ` +
      `Parsing ${frameCount} complete frames, ignoring trailing ${totalBytes % FRAME_SIZE} bytes.`
    );
  }

  const dataView = new DataView(buffer);
  const frames = [];

  for (let f = 0; f < frameCount; f++) {
    const frameOffset = f * FRAME_SIZE;
    const imageData = new ImageData(FRAME_WIDTH, FRAME_HEIGHT);
    const rgba = imageData.data;

    for (let p = 0; p < FRAME_WIDTH * FRAME_HEIGHT; p++) {
      // Read 16-bit little-endian pixel
      const pixel = dataView.getUint16(frameOffset + p * BYTES_PER_PIXEL, true);
      const { r, g, b, a } = rgb565ToRGBA(pixel);

      const idx = p * 4;
      rgba[idx] = r;
      rgba[idx + 1] = g;
      rgba[idx + 2] = b;
      rgba[idx + 3] = a;
    }

    frames.push(imageData);
  }

  return frames;
}

/**
 * Constants exported for use by other modules.
 */
export const SPRITE_CONSTANTS = {
  FRAME_WIDTH,
  FRAME_HEIGHT,
  BYTES_PER_PIXEL,
  FRAME_SIZE,
  DEFAULT_FPS: 24,
};