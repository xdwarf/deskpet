// =============================================================================
// DeskPet — sprite_manager.cpp
// =============================================================================
// Downloads and caches Muni's sprite sheets from the sprite server.
// See sprite_manager.h for the full design overview.
//
// STORAGE SPLIT:
//   SD card  — actual .sprite files under /sprites/<character>/
//   NVS      — just the cached version string (Preferences, survives reboot)
//
// sdInit() must be called before spriteManagerInit().
// If the SD card is not mounted (sdAvailable() == false), sprite caching is
// skipped entirely and the device falls back to programmatic face drawing.
//
// SPRITE_DOWNLOAD_ENABLED
// =======================
// Set to 1 to re-enable manifest fetch and sprite download on boot.
// While 0, spriteManagerInit() only scans the SD card for existing files
// and reports what it finds — no network requests are made.
// =============================================================================
#define SPRITE_DOWNLOAD_ENABLED 0

#include <Arduino.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include <Preferences.h>    // Arduino wrapper around ESP-IDF NVS
#include <ArduinoJson.h>
#include "sprite_manager.h"
#include "sd_card.h"
#include "config.h"
#include "wifi_manager.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool s_hasSprites       = false;
static char s_cachedVersion[32] = "none";

// Static path buffer — reused by spriteManagerPath()
static char s_pathBuf[64];

// ---------------------------------------------------------------------------
// NVS helpers — version string only
// ---------------------------------------------------------------------------
static void nvsReadVersion(char* outBuf, size_t bufLen) {
    Preferences prefs;
    // Open read-write (readOnly=false) so ESP-IDF creates the namespace in NVS
    // flash if it doesn't exist yet. Opening a non-existent namespace as
    // read-only returns NOT_FOUND and prefs.getString() returns the default,
    // but also logs an error and leaves the namespace uncreated — meaning the
    // same error fires on every subsequent boot until a write finally creates
    // it. Opening read-write is safe here: we only read, but the namespace
    // is initialised so the next write succeeds silently.
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
        Serial.println("[Sprites] NVS open failed — treating cached version as 'none'");
        strncpy(outBuf, "none", bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return;
    }
    String v = prefs.getString(NVS_KEY_SPRITE_VERSION, "none");
    prefs.end();
    strncpy(outBuf, v.c_str(), bufLen - 1);
    outBuf[bufLen - 1] = '\0';
}

static void nvsWriteVersion(const char* version) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    prefs.putString(NVS_KEY_SPRITE_VERSION, version);
    prefs.end();
    Serial.printf("[Sprites] NVS version updated to '%s'\n", version);
}

// ---------------------------------------------------------------------------
// SD card helpers
// ---------------------------------------------------------------------------

// Ensure /sprites/<character>/ exists on the SD card
static const char* SD_ROOT = "/sdcard";

static void ensureDir() {
    char dir[64];
    snprintf(dir, sizeof(dir), "%s/sprites/%s", SD_ROOT, SPRITE_CHARACTER);
    if (!SD_MMC.exists(String(SD_ROOT) + "/sprites")) {
        SD_MMC.mkdir(String(SD_ROOT) + "/sprites");
    }
    if (!SD_MMC.exists(dir)) {
        SD_MMC.mkdir(dir);
        Serial.printf("[Sprites] Created SD directory %s\n", dir);
    }
}

// Returns true if the sprite file for a given expression exists on the SD card
static bool spriteFileExists(const char* exprName) {
    return SD_MMC.exists(spriteManagerPath(exprName));
}

// Check whether any sprite file for the current character is on the SD card
static bool anySpriteCached() {
    const char* expressions[] = {
        "neutral", "happy", "sad", "surprised", "sleepy", "excited", "thinking"
    };
    for (const char* expr : expressions) {
        if (spriteFileExists(expr)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// HTTP download helpers
// ---------------------------------------------------------------------------

// Download a URL and return the response body as a String.
// Returns an empty string on failure.
static String httpGetString(const char* url) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Sprites] GET %s → HTTP %d\n", url, code);
        http.end();
        return "";
    }

    String body = http.getString();
    http.end();
    return body;
}

// Download a URL and save the response body to a path on the SD card.
// Returns true on success.
static bool httpDownloadToFile(const char* url, const char* sdPath) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000); // sprite files can be ~113 KB — allow more time

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Sprites] GET %s → HTTP %d\n", url, code);
        http.end();
        return false;
    }

    File f = SD_MMC.open(sdPath, FILE_WRITE);
    if (!f) {
        Serial.printf("[Sprites] Cannot open SD:%s for writing\n", sdPath);
        http.end();
        return false;
    }

    // Stream the response body directly into the file in 512-byte chunks
    // to avoid allocating a large heap buffer.
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    int totalBytes = 0;
    int available  = http.getSize(); // -1 if chunked

    while (http.connected() && (available > 0 || available == -1)) {
        size_t toRead = stream->available();
        if (toRead > 0) {
            size_t readNow = (toRead > sizeof(buf)) ? sizeof(buf) : toRead;
            int n = stream->readBytes(buf, readNow);
            f.write(buf, n);
            totalBytes += n;
            if (available > 0) available -= n;
        } else {
            delay(1); // yield while waiting for data
        }
    }

    f.close();
    http.end();
    Serial.printf("[Sprites] Saved SD:%s (%d bytes)\n", sdPath, totalBytes);
    return totalBytes > 0;
}

// ---------------------------------------------------------------------------
// Download all sprite files for the active character to the SD card
// ---------------------------------------------------------------------------
static bool downloadSprites(const char* expressions[], int count) {
    bool allOk = true;

    for (int i = 0; i < count; i++) {
        const char* expr = expressions[i];
        const char* path = spriteManagerPath(expr);

        char url[128];
        snprintf(url, sizeof(url), "%s/%s/%s.sprite",
                 SPRITE_SERVER_BASE_URL, SPRITE_CHARACTER, expr);

        Serial.printf("[Sprites] Downloading %s → SD:%s\n", url, path);
        if (!httpDownloadToFile(url, path)) {
            Serial.printf("[Sprites] Failed to download %s — skipping\n", expr);
            allOk = false;
            // Continue anyway — partial cache is still useful
        }
    }

    return allOk;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void spriteManagerInit() {
    Serial.println("[Sprites] Initialising...");

    // SD card must be mounted before we can cache anything
    if (!sdAvailable()) {
        Serial.println("[Sprites] SD card not available — skipping sprite cache");
        return;
    }

    ensureDir();

    // --- Read cached version from NVS ---
    nvsReadVersion(s_cachedVersion, sizeof(s_cachedVersion));
    Serial.printf("[Sprites] Cached version: %s\n", s_cachedVersion);

    // --- Check if we already have sprites to fall back on ---
    s_hasSprites = anySpriteCached();
    Serial.printf("[Sprites] SD sprites present: %s\n", s_hasSprites ? "yes" : "no");

#if SPRITE_DOWNLOAD_ENABLED
    // --- Skip server check if WiFi is offline ---
    if (!wifiIsConnected()) {
        Serial.println("[Sprites] WiFi offline — using cached sprites");
        return;
    }

    // --- Fetch manifest from server ---
    Serial.printf("[Sprites] Fetching manifest from %s\n", SPRITE_SERVER_MANIFEST_URL);
    String body = httpGetString(SPRITE_SERVER_MANIFEST_URL);

    if (body.isEmpty()) {
        Serial.println("[Sprites] Manifest fetch failed — using cached sprites");
        return;
    }

    // --- Parse manifest JSON ---
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Sprites] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* serverVersion = doc["version"] | "0.0.0";
    Serial.printf("[Sprites] Server version: %s\n", serverVersion);

    // --- Compare versions ---
    if (strcmp(serverVersion, s_cachedVersion) == 0) {
        Serial.println("[Sprites] Version matches cache — no download needed");
        s_hasSprites = anySpriteCached();
        return;
    }

    // --- New version available — download sprites to SD card ---
    Serial.printf("[Sprites] New version %s available (cached: %s) — downloading\n",
                  serverVersion, s_cachedVersion);

    // Build expression list from manifest (or use hardcoded fallback)
    const char* expressions[16];
    int count = 0;
    JsonArray exprArray = doc["expressions"].as<JsonArray>();
    for (JsonVariant v : exprArray) {
        if (count < 16) {
            expressions[count++] = v.as<const char*>();
        }
    }
    if (count == 0) {
        // Fallback if manifest has no expression list
        const char* fallback[] = {
            "neutral", "happy", "sad", "surprised", "sleepy", "excited", "thinking"
        };
        for (int i = 0; i < 7; i++) expressions[i] = fallback[i];
        count = 7;
    }

    bool allDownloaded = downloadSprites(expressions, count);
    s_hasSprites = anySpriteCached();

    if (allDownloaded) {
        // Only update NVS if every file downloaded successfully, so a partial
        // failure forces a re-download on next boot rather than silently missing
        // expressions.
        nvsWriteVersion(serverVersion);
        strncpy(s_cachedVersion, serverVersion, sizeof(s_cachedVersion) - 1);
        Serial.println("[Sprites] All sprites downloaded and cached to SD card");
    } else {
        Serial.println("[Sprites] Some downloads failed — version NOT updated in NVS");
    }
#else
    Serial.println("[Sprites] Download disabled (SPRITE_DOWNLOAD_ENABLED=0) — using SD card files as-is");
#endif
}

// ---------------------------------------------------------------------------
bool spriteManagerHasSprites() {
    return s_hasSprites;
}

// ---------------------------------------------------------------------------
const char* spriteManagerPath(const char* expressionName) {
    snprintf(s_pathBuf, sizeof(s_pathBuf), "%s/sprites/%s/%s.sprite",
             SD_ROOT, SPRITE_CHARACTER, expressionName);
    return s_pathBuf;
}

// ---------------------------------------------------------------------------
const char* spriteManagerCachedVersion() {
    return s_cachedVersion;
}
