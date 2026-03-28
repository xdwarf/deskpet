/**
 * StatusBar.jsx
 * 
 * Top bar showing connection status indicators:
 *   - Sprite Server status (green/red dot)
 *   - Live Preview WebSocket status (green/red dot)
 *   - Refresh button to re-fetch the manifest
 */

import React from 'react';
import { RefreshCw } from 'lucide-react';
import { Button } from '@/components/ui/button';

function StatusDot({ status }) {
  const isConnected = status === 'connected';
  const isLoading = status === 'loading' || status === 'connecting';

  return (
    <span
      className={`inline-block w-2.5 h-2.5 rounded-full transition-colors duration-300 ${
        isConnected
          ? 'bg-green-500 shadow-[0_0_6px_rgba(34,197,94,0.6)]'
          : isLoading
          ? 'bg-yellow-500 shadow-[0_0_6px_rgba(234,179,8,0.6)]'
          : 'bg-red-500 shadow-[0_0_6px_rgba(239,68,68,0.6)]'
      }`}
    />
  );
}

export default function StatusBar({ serverStatus, wsStatus, onRefresh, isRefreshing }) {
  return (
    <div className="flex items-center justify-between px-5 py-3 bg-norse-panel border-b border-border">
      {/* Left: App title */}
      <div className="flex items-center gap-3">
        <span className="text-norse-gold font-mono font-bold text-sm tracking-wider">
          MUNI EMULATOR
        </span>
        <span className="text-muted-foreground font-mono text-xs">v0.1.0</span>
      </div>

      {/* Right: Status indicators + refresh */}
      <div className="flex items-center gap-5">
        {/* Sprite Server status */}
        <div className="flex items-center gap-2">
          <StatusDot status={serverStatus} />
          <span className="text-xs font-mono text-norse-text opacity-70">Sprite Server</span>
        </div>

        {/* WebSocket status */}
        <div className="flex items-center gap-2">
          <StatusDot status={wsStatus} />
          <span className="text-xs font-mono text-norse-text opacity-70">Live Preview (WS)</span>
        </div>

        {/* Refresh button */}
        <Button
          variant="ghost"
          size="sm"
          onClick={onRefresh}
          disabled={isRefreshing}
          className="text-norse-gold hover:text-norse-text hover:bg-norse-hover h-7 px-2"
        >
          <RefreshCw className={`w-3.5 h-3.5 ${isRefreshing ? 'animate-spin' : ''}`} />
          <span className="ml-1.5 text-xs font-mono">Refresh</span>
        </Button>
      </div>
    </div>
  );
}