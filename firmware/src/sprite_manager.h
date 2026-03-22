#pragma once

// =============================================================================
// DeskPet — sprite_manager.h
// =============================================================================
// Manages downloading and caching of Muni's sprite sheets.
//
// HOW IT WORKS
// ============
// On boot (call spriteManagerInit()):
//   1. Check sdAvailable() — if SD is not mounted, skip all caching.
//   2. Fetch manifest.json from SPRITE_SERVER_MANIFEST_URL over HTTP.
//   3. Compare the "version" field against the version string stored in NVS
//      (non-volatile storage — survives reboots and power cycles).
//   4. If the server version is newer (or NVS has no stored version):
//        - Download each sprite sheet listed in the manifest to the SD card.
//        - Update the NVS version string only if all files downloaded OK.
//   5. If versions match, use the files already cached on the SD card.
//   6. If WiFi is offline or the server is unreachable:
//        - Fall back gracefully to the programmatic face in expressions.cpp.
//        - Do NOT block boot or crash.
//
// STORAGE SPLIT
// =============
//   SD card  — sprite files: /sprites/<character>/<expression>.sprite
//              e.g. /sprites/muni/neutral.sprite
//              Each file is raw RGB565: 240×240×2 = 115,200 bytes per frame,
//              frames concatenated with no header.
//
//   NVS      — version string only (Preferences, key "sprite_ver").
//              Tells us whether the cached files are up to date.
//              LittleFS is NOT used by this module.
//
// DEPENDENCIES
// ============
//   sdInit() must be called before spriteManagerInit().
//   wifiConnect() must be called before spriteManagerInit() for downloads
//   to succeed; the module falls back gracefully if WiFi is offline.
//
// FALLBACK BEHAVIOUR
// ==================
// spriteManagerHasSprites() returns false if:
//   - SD card is not mounted
//   - Server was unreachable and no files were cached
//   - The sprite file for a given expression doesn't exist
// expressions.cpp checks this before deciding whether to push a cached bitmap
// or call drawFace().
// =============================================================================

// Initialise sprite cache: check SD, compare manifest version, download if
// newer. Call once in setup(), after sdInit() and wifiConnect().
// Non-fatal if the server is unreachable — falls back to cached SD files.
void spriteManagerInit();

// Returns true if at least one sprite file for the active character is
// available on the SD card.
bool spriteManagerHasSprites();

// Returns the SD card path to the sprite file for a given expression name.
// e.g. spriteManagerPath("happy") → "/sprites/muni/happy.sprite"
// The returned pointer is valid for the lifetime of the program (static buffer).
const char* spriteManagerPath(const char* expressionName);

// Returns the version string currently cached in NVS (e.g. "0.0.1"),
// or "none" if no version has been stored yet.
const char* spriteManagerCachedVersion();
