// =============================================================================
// DeskPet — sprite_manager.cpp
// =============================================================================
// Downloads and caches Muni's sprite sheets from sprites.mael.dk.
// See sprite_manager.h for the full design overview.
// =============================================================================

#include <Arduino.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <Preferences.h>    // Arduino wrapper around ESP-IDF NVS
#include <ArduinoJson.h>
#include "sprite_manager.h"
#include "config.h"
#include "wifi_manager.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool   s_hasSprites    = false;  // true once at least one file exists
static char   s_cachedVersion[32] = "none";

// Static path buffer — reused by spriteManagerPath()
static char   s_pathBuf[64];

// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------
static void nvsReadVersion(char* outBuf, size_t bufLen) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
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
// LittleFS helpers
// ---------------------------------------------------------------------------

// Ensure /sprites/<character>/ exists in LittleFS
static void ensureDir() {
    char dir[48];
    snprintf(dir, sizeof(dir), "/sprites/%s", SPRITE_CHARACTER);
    if (!LittleFS.exists(dir)) {
        LittleFS.mkdir("/sprites");
        LittleFS.mkdir(dir);
        Serial.printf("[Sprites] Created directory %s\n", dir);
    }
}

// Returns true if the sprite file for a given expression exists in LittleFS
static bool spriteFileExists(const char* exprName) {
    const char* path = spriteManagerPath(exprName);
    return LittleFS.exists(path);
}

// Check if any sprite file exists for the current character
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

// Download a URL and save the response body to a LittleFS path.
// Returns true on success.
static bool httpDownloadToFile(const char* url, const char* fsPath) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000); // sprite files can be ~113 KB — allow more time

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Sprites] GET %s → HTTP %d\n", url, code);
        http.end();
        return false;
    }

    File f = LittleFS.open(fsPath, FILE_WRITE);
    if (!f) {
        Serial.printf("[Sprites] Cannot open %s for writing\n", fsPath);
        http.end();
        return false;
    }

    // Stream the response body directly into the file in 512-byte chunks
    // to avoid allocating a large buffer in heap.
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
    Serial.printf("[Sprites] Saved %s (%d bytes)\n", fsPath, totalBytes);
    return totalBytes > 0;
}

// ---------------------------------------------------------------------------
// Download all sprite files for the active character
// ---------------------------------------------------------------------------
static bool downloadSprites(const char* expressions[], int count) {
    bool allOk = true;

    for (int i = 0; i < count; i++) {
        const char* expr = expressions[i];
        const char* path = spriteManagerPath(expr);

        char url[128];
        snprintf(url, sizeof(url), "%s/%s/%s.sprite",
                 SPRITE_SERVER_BASE_URL, SPRITE_CHARACTER, expr);

        Serial.printf("[Sprites] Downloading %s → %s\n", url, path);
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

    // --- Mount LittleFS ---
    if (!LittleFS.begin(/*formatOnFail=*/true)) {
        Serial.println("[Sprites] LittleFS mount failed — no sprite caching");
        return;
    }
    Serial.println("[Sprites] LittleFS mounted");

    ensureDir();

    // --- Read cached version from NVS ---
    nvsReadVersion(s_cachedVersion, sizeof(s_cachedVersion));
    Serial.printf("[Sprites] Cached version: %s\n", s_cachedVersion);

    // --- Check if we have any sprites to fall back on ---
    s_hasSprites = anySpriteCached();

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

    // --- New version available — download sprites ---
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
        Serial.println("[Sprites] All sprites downloaded and cached");
    } else {
        Serial.println("[Sprites] Some downloads failed — version NOT updated in NVS");
    }
}

// ---------------------------------------------------------------------------
bool spriteManagerHasSprites() {
    return s_hasSprites;
}

// ---------------------------------------------------------------------------
const char* spriteManagerPath(const char* expressionName) {
    snprintf(s_pathBuf, sizeof(s_pathBuf), "/sprites/%s/%s.sprite",
             SPRITE_CHARACTER, expressionName);
    return s_pathBuf;
}

// ---------------------------------------------------------------------------
const char* spriteManagerCachedVersion() {
    return s_cachedVersion;
}
