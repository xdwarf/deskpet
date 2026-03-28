/**
 * RoundDisplay.jsx
 * 
 * The centrepiece: a 240×240 round display emulator.
 * 
 * Features:
 *   - HTML5 Canvas clipped to a circle via CSS border-radius
 *   - Pulsing blue CSS box-shadow mimicking WS2812B LEDs
 *   - Renders sprite animation frames at 10fps
 *   - Shows placeholder rune text when no sprite is loaded
 */

import React, { useRef, useEffect, useCallback } from 'react';
import { SPRITE_CONSTANTS } from '@/lib/spriteParser';

const { FRAME_WIDTH, FRAME_HEIGHT, DEFAULT_FPS } = SPRITE_CONSTANTS;
const FRAME_INTERVAL = 1000 / DEFAULT_FPS; // 100ms per frame at 10fps

export default function RoundDisplay({ frames, activeExpression }) {
  const canvasRef = useRef(null);
  const animFrameRef = useRef(null);
  const frameIndexRef = useRef(0);
  const lastFrameTimeRef = useRef(0);

  /**
   * Draw the placeholder rune text when no sprite is loaded.
   */
  const drawPlaceholder = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // Clear with background colour
    ctx.fillStyle = '#1A1A2E';
    ctx.fillRect(0, 0, FRAME_WIDTH, FRAME_HEIGHT);

    // Draw rune text
    ctx.fillStyle = '#C9A84C';
    ctx.font = '32px serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('ᛗᚢᚾᛁ', FRAME_WIDTH / 2, FRAME_HEIGHT / 2 - 10);

    // Subtitle
    ctx.font = '11px monospace';
    ctx.fillStyle = 'rgba(201, 168, 76, 0.5)';
    ctx.fillText('awaiting sprite...', FRAME_WIDTH / 2, FRAME_HEIGHT / 2 + 25);
  }, []);

  /**
   * Animation loop: renders frames at the configured FPS.
   */
  const animate = useCallback(
    (timestamp) => {
      if (!frames || frames.length === 0) return;

      const elapsed = timestamp - lastFrameTimeRef.current;

      if (elapsed >= FRAME_INTERVAL) {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');

        // Draw current frame
        ctx.putImageData(frames[frameIndexRef.current], 0, 0);

        // Advance to next frame (loop)
        frameIndexRef.current = (frameIndexRef.current + 1) % frames.length;
        lastFrameTimeRef.current = timestamp;
      }

      animFrameRef.current = requestAnimationFrame(animate);
    },
    [frames]
  );

  // Start / stop animation when frames change
  useEffect(() => {
    // Cancel any running animation
    if (animFrameRef.current) {
      cancelAnimationFrame(animFrameRef.current);
      animFrameRef.current = null;
    }

    // Reset frame index
    frameIndexRef.current = 0;
    lastFrameTimeRef.current = 0;

    if (frames && frames.length > 0) {
      // Start animation loop
      animFrameRef.current = requestAnimationFrame(animate);
    } else {
      // Show placeholder
      drawPlaceholder();
    }

    return () => {
      if (animFrameRef.current) {
        cancelAnimationFrame(animFrameRef.current);
      }
    };
  }, [frames, animate, drawPlaceholder]);

  return (
    <div className="flex-1 flex flex-col items-center justify-center bg-norse-bg">
      {/* Display container with LED glow */}
      <div className="relative">
        {/* Outer bezel ring */}
        <div className="rounded-full p-1 bg-gradient-to-br from-gray-700 via-gray-800 to-gray-900">
          <canvas
            ref={canvasRef}
            width={FRAME_WIDTH}
            height={FRAME_HEIGHT}
            className="block rounded-full led-pulse"
            style={{
              width: '280px',
              height: '280px',
              imageRendering: 'pixelated',
            }}
          />
        </div>
      </div>

      {/* Expression label below display */}
      <div className="mt-6 text-center">
        {activeExpression ? (
          <span className="font-mono text-norse-gold text-sm tracking-wider">
            {activeExpression}
          </span>
        ) : (
          <span className="font-mono text-muted-foreground text-xs">
            Select an expression to play
          </span>
        )}
      </div>

      {/* Frame count indicator */}
      {frames && frames.length > 0 && (
        <div className="mt-2 font-mono text-[10px] text-muted-foreground">
          {frames.length} frame{frames.length !== 1 ? 's' : ''} · {DEFAULT_FPS}fps
        </div>
      )}
    </div>
  );
}