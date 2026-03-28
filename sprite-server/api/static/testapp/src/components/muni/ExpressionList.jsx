/**
 * ExpressionList.jsx
 * 
 * Left panel showing the list of available expressions
 * fetched from the sprite server manifest.
 * 
 * - Displays "Muni" as the character header
 * - Lists all expressions with click-to-play
 * - Highlights the currently active expression in gold
 */

import React from 'react';
import { Loader2 } from 'lucide-react';

export default function ExpressionList({
  expressions,
  activeExpression,
  loadingExpression,
  onSelect,
  serverStatus,
}) {
  return (
    <div className="w-56 flex-shrink-0 bg-norse-panel border-r border-border flex flex-col h-full">
      {/* Character header */}
      <div className="px-4 py-4 border-b border-border">
        <h2 className="text-norse-gold font-mono font-bold text-lg tracking-widest">
          ᛗ Muni
        </h2>
        <p className="text-muted-foreground font-mono text-[10px] mt-1">
          {expressions.length} expressions
        </p>
      </div>

      {/* Expression list */}
      <div className="flex-1 overflow-y-auto expression-list py-2">
        {serverStatus === 'loading' && (
          <div className="flex items-center justify-center py-8">
            <Loader2 className="w-5 h-5 text-norse-gold animate-spin" />
          </div>
        )}

        {serverStatus === 'disconnected' && expressions.length === 0 && (
          <div className="px-4 py-8 text-center">
            <p className="text-muted-foreground font-mono text-xs">
              Unable to reach sprite server
            </p>
          </div>
        )}

        {expressions.map((expr) => {
          const isActive = activeExpression === expr;
          const isLoading = loadingExpression === expr;

          return (
            <button
              key={expr}
              onClick={() => onSelect(expr)}
              disabled={isLoading}
              className={`w-full text-left px-4 py-2 font-mono text-sm transition-all duration-200 flex items-center gap-2 ${
                isActive
                  ? 'text-norse-gold bg-norse-hover border-l-2 border-norse-gold'
                  : 'text-norse-text opacity-70 hover:opacity-100 hover:bg-norse-hover border-l-2 border-transparent'
              }`}
            >
              {isLoading && (
                <Loader2 className="w-3 h-3 animate-spin flex-shrink-0" />
              )}
              <span className="truncate">{expr}</span>
            </button>
          );
        })}
      </div>
    </div>
  );
}