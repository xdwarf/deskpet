/**
 * useSpriteServer.js
 * 
 * Custom hook to manage sprite server connection:
 *   - Fetches the manifest from the sprite server
 *   - Provides expression list and server status
 */

import { useState, useCallback } from 'react';

const MANIFEST_URL = 'http://sprites.mael.dk/manifest.json';
const SPRITE_BASE_URL = 'http://sprites.mael.dk/muni/';

export function useSpriteServer() {
  const [expressions, setExpressions] = useState([]);
  const [serverStatus, setServerStatus] = useState('disconnected'); // 'connected' | 'disconnected' | 'loading'
  const [error, setError] = useState(null);

  /**
   * Fetch the manifest.json from the sprite server.
   * Populates the expression list and updates server status.
   */
  const fetchManifest = useCallback(async () => {
    setServerStatus('loading');
    setError(null);

    try {
      const response = await fetch(MANIFEST_URL);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const manifest = await response.json();
      const expressionList = manifest.expressions || [];
      setExpressions(expressionList);
      setServerStatus('connected');
      return expressionList;
    } catch (err) {
      console.error('[useSpriteServer] Failed to fetch manifest:', err);
      setError(err.message);
      setServerStatus('disconnected');
      return [];
    }
  }, []);

  /**
   * Fetch a .sprite binary file for the given expression name.
   * 
   * @param {string} expressionName - Name of the expression
   * @returns {Promise<ArrayBuffer|null>} Raw sprite data or null on error
   */
  const fetchSprite = useCallback(async (expressionName) => {
    try {
      const url = `${SPRITE_BASE_URL}${expressionName}.sprite`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      return await response.arrayBuffer();
    } catch (err) {
      console.error(`[useSpriteServer] Failed to fetch sprite "${expressionName}":`, err);
      return null;
    }
  }, []);

  return {
    expressions,
    serverStatus,
    error,
    fetchManifest,
    fetchSprite,
  };
}