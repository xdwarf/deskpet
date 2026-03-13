#pragma once

// =============================================================================
// DeskPet — sprite_manager.h
// =============================================================================
// Manages downloading and caching of Muni's sprite sheets.
//
// HOW IT WORKS
// ============
// On boot (call spriteManagerInit()):
//   1. Mount LittleFS — this is the ESP32's on-flash filesystem.
//   2. Fetch manifest.json from SPRITE_SERVER_MANIFEST_URL over HTTP.
//   3. Compare the "version" field against the version string stored in NVS
//      (non-volatile storage — survives reboots and power cycles).
//   4. If the server version is newer (or NVS has no stored version):
//        - Download each sprite sheet listed in the manifest to LittleFS.
//        - Update the NVS version string on success.
//   5. If versions match, use the files already cached in LittleFS.
//   6. If WiFi is offline or the server is unreachable:
//        - Fall back gracefully to the programmatic face in expressions.cpp.
//        - Do NOT block boot or crash.
//
// SPRITE FILES ON LITTLEFS
// ========================
// Sprites are stored as flat binary files under /sprites/<character>/:
//   /sprites/muni/neutral.sprite
//   /sprites/muni/happy.sprite
//   ... etc.
//
// Each .sprite file is a raw RGB565 bitmap: 240×240 × 2 bytes = 115,200 bytes.
// Rendered by pushing directly to the display via sprite.pushImage() or
// tft.pushImage(), bypassing the programmatic drawFace() call.
//
// FALLBACK BEHAVIOUR
// ==================
// spriteManagerHasSprites() returns false if:
//   - LittleFS mount failed
//   - Server was unreachable and no files were cached
//   - The sprite file for a given expression doesn't exist
// expressions.cpp checks this before deciding whether to push a cached bitmap
// or call drawFace().
// =============================================================================

// Initialise LittleFS, check the sprite manifest, and update cached files
// if the server has a newer version.
// Call once in setup(), after WiFi is connected.
// Non-fatal if the server is unreachable — falls back to cached files.
void spriteManagerInit();

// Returns true if at least one sprite file for the active character is
// available in LittleFS. Use this to decide between bitmap and programmatic
// rendering in expressions.cpp.
bool spriteManagerHasSprites();

// Returns the path in LittleFS to the sprite file for a given expression name.
// e.g. spriteManagerPath("happy") → "/sprites/muni/happy.sprite"
// The returned pointer is valid for the lifetime of the program (static buffer).
const char* spriteManagerPath(const char* expressionName);

// Returns the version string currently cached in NVS (e.g. "0.0.1"),
// or "none" if no version has been stored yet.
const char* spriteManagerCachedVersion();
