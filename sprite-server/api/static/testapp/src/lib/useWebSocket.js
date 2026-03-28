/**
 * useWebSocket.js
 * 
 * Custom hook for the live preview WebSocket connection.
 * Connects to the Muni device's WebSocket server and listens
 * for expression change messages.
 */

import { useState, useEffect, useRef, useCallback } from 'react';

const WS_URL = 'ws://192.168.2.14:8765';
const RECONNECT_INTERVAL = 5000; // Retry every 5 seconds

export function useWebSocket(onExpressionChange) {
  const [wsStatus, setWsStatus] = useState('disconnected'); // 'connected' | 'disconnected' | 'connecting'
  const wsRef = useRef(null);
  const reconnectTimerRef = useRef(null);
  const onExpressionChangeRef = useRef(onExpressionChange);

  // Keep callback ref current
  onExpressionChangeRef.current = onExpressionChange;

  const connect = useCallback(() => {
    // Clean up any existing connection
    if (wsRef.current) {
      wsRef.current.close();
    }

    setWsStatus('connecting');

    try {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        console.log('[WebSocket] Connected to', WS_URL);
        setWsStatus('connected');
      };

      ws.onmessage = (event) => {
        const message = event.data.trim();
        console.log('[WebSocket] Received:', message);

        // Notify parent of the expression change
        if (message && onExpressionChangeRef.current) {
          onExpressionChangeRef.current(message);
        }
      };

      ws.onclose = () => {
        console.log('[WebSocket] Disconnected');
        setWsStatus('disconnected');

        // Schedule reconnect
        reconnectTimerRef.current = setTimeout(() => {
          console.log('[WebSocket] Attempting reconnect...');
          connect();
        }, RECONNECT_INTERVAL);
      };

      ws.onerror = (err) => {
        console.error('[WebSocket] Error:', err);
        // onclose will fire after this, triggering reconnect
      };
    } catch (err) {
      console.error('[WebSocket] Failed to create connection:', err);
      setWsStatus('disconnected');

      // Schedule reconnect
      reconnectTimerRef.current = setTimeout(connect, RECONNECT_INTERVAL);
    }
  }, []);

  useEffect(() => {
    connect();

    return () => {
      // Cleanup on unmount
      if (reconnectTimerRef.current) {
        clearTimeout(reconnectTimerRef.current);
      }
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [connect]);

  return { wsStatus };
}