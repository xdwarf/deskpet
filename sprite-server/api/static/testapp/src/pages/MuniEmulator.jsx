/**
 * MuniEmulator.jsx
 * 
 * Main page for the Muni Emulator — a developer tool that emulates
 * the Muni 240×240 round display on PC, fetching and playing
 * .sprite animations from the sprite server.
 * 
 * Layout: three sections horizontally
 *   1. Left panel  — expression list (fetched from server)
 *   2. Centre      — the round display emulator
 *   3. Top bar     — connection status
 */

import React, { useState, useEffect, useCallback } from 'react';
import { parseSpriteFile } from '@/lib/spriteParser';
import { useSpriteServer } from '@/lib/useSpriteServer';
import { useWebSocket } from '@/lib/useWebSocket';
import StatusBar from '@/components/muni/StatusBar';
import ExpressionList from '@/components/muni/ExpressionList';
import RoundDisplay from '@/components/muni/RoundDisplay';

export default function MuniEmulator() {
  const [activeExpression, setActiveExpression] = useState(null);
  const [loadingExpression, setLoadingExpression] = useState(null);
  const [frames, setFrames] = useState(null);
  const [isRefreshing, setIsRefreshing] = useState(false);

  const {
    expressions,
    serverStatus,
    fetchManifest,
    fetchSprite,
  } = useSpriteServer();

  /**
   * Load and parse a sprite for the given expression name.
   */
  const loadExpression = useCallback(async (expressionName) => {
    // Don't reload if already active
    if (expressionName === activeExpression && frames) return;

    setLoadingExpression(expressionName);
    setActiveExpression(expressionName);

    const buffer = await fetchSprite(expressionName);

    if (buffer) {
      const parsedFrames = parseSpriteFile(buffer);
      setFrames(parsedFrames.length > 0 ? parsedFrames : null);
    } else {
      setFrames(null);
    }

    setLoadingExpression(null);
  }, [activeExpression, frames, fetchSprite]);

  /**
   * WebSocket expression change handler.
   * If the received message matches a known expression, switch to it.
   */
  const handleWsExpressionChange = useCallback(
    (message) => {
      if (expressions.includes(message)) {
        loadExpression(message);
      } else {
        console.log(`[MuniEmulator] WS message "${message}" not in expression list, ignoring.`);
      }
    },
    [expressions, loadExpression]
  );

  const { wsStatus } = useWebSocket(handleWsExpressionChange);

  /**
   * Fetch manifest on initial load.
   */
  useEffect(() => {
    fetchManifest();
  }, [fetchManifest]);

  /**
   * Handle refresh button click.
   */
  const handleRefresh = async () => {
    setIsRefreshing(true);
    await fetchManifest();
    setIsRefreshing(false);
  };

  return (
    <div className="h-screen flex flex-col overflow-hidden bg-norse-bg">
      {/* Top status bar */}
      <StatusBar
        serverStatus={serverStatus}
        wsStatus={wsStatus}
        onRefresh={handleRefresh}
        isRefreshing={isRefreshing}
      />

      {/* Main content area */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left panel: expression list */}
        <ExpressionList
          expressions={expressions}
          activeExpression={activeExpression}
          loadingExpression={loadingExpression}
          onSelect={loadExpression}
          serverStatus={serverStatus}
        />

        {/* Centre: round display emulator */}
        <RoundDisplay
          frames={frames}
          activeExpression={activeExpression}
        />
      </div>
    </div>
  );
}