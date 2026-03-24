# DeskPet — Muni

> Muninn, Odin's raven, rendered in chibi Norse/Viking style. Watching over your desk.

Muni lives on a round 240×240 display and reacts to your smart home, your streams, and your life. Happy when you arrive home. Sleepy when everyone's in bed. Surprised when a door opens. Each expression plays through a hand-drawn sprite animation, then he returns to his calm idle loop. Runs on an ESP32-C3 SuperMini — an ESP32-S3 N16R8 upgrade is in the post.

**GitHub:** [github.com/xdwarf/deskpet](https://github.com/xdwarf/deskpet)

---

## Repository Structure

```
deskpet/
├── firmware/                    # PlatformIO project (C++ / Arduino framework)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── display.cpp/.h       # LovyanGFX display driver wrapper
│   │   ├── expressions.cpp/.h   # Expression state machine + programmatic face
│   │   ├── sprite_player.cpp/.h # SD card sprite streaming (chunked RGB565)
│   │   ├── sprite_manager.cpp/.h# SD scan, NVS version cache, download (disabled)
│   │   ├── wifi_manager.cpp/.h  # WiFi connection + auto-reconnect
│   │   ├── mqtt_client.cpp/.h   # MQTT subscribe/publish
│   │   ├── sd_card.cpp/.h       # SD card init on shared SPI2 bus
│   │   └── leds.cpp/.h          # WS2812B midnight-blue breathing (NeoPixelBus)
│   ├── include/
│   │   ├── config.example.h     # Template — copy to config.h and fill in values
│   │   ├── config.h             # WiFi/MQTT credentials + pin defines (gitignored)
│   │   └── lgfx_config.h        # LovyanGFX panel + SPI bus configuration
│   └── platformio.ini
├── sprite-server/               # Docker service — GIF→RGB565 converter + file server
│   ├── api/                     # FastAPI conversion service (port 8000)
│   │   └── static/              # Web UI — upload, convert, publish, download
│   ├── sprites/                 # Served sprite files + manifest.json
│   └── docker-compose.yaml      # nginx on 8765, api on 8000
├── simulator/                   # Python/pygame desktop simulator (round display)
├── muni.code-workspace          # VS Code multi-root workspace — open this
└── README.md
```

---

## Hardware

### Current build

| Component | Part | Notes |
|---|---|---|
| Microcontroller | ESP32-C3 SuperMini | Single-core RISC-V, 400 KB SRAM, WiFi, tiny form factor |
| Display | GC9A01 240×240 round SPI | LovyanGFX driver, 40 MHz, `invert=true`, RGB565 |
| Storage | Micro SD card module | Shares SPI2 with display; FAT32, up to 32 GB |
| Ambient LEDs | 3× WS2812B | GPIO9, 5V, NeoPixelBus RMT driver, midnight-blue breathing |

### Ordered — upgrade board

| Component | Part | Notes |
|---|---|---|
| Microcontroller | ESP32-S3 N16R8 | Dual-core, 16 MB flash, **8 MB PSRAM** — enables full-frame sprite buffer and faster playback |

When the S3 arrives: swap boards, re-enable `SPRITE_DOWNLOAD_ENABLED`, use PSRAM for a full 115 KB frame buffer, remove the chunked-read workaround.

### Home server (192.168.2.14)

- Mosquitto MQTT broker — port 1883
- Docker host — sprite server (ports 8000 + 8765)

---

## Wiring

### Full pin table

| Signal | ESP32-C3 GPIO | Connected to |
|---|---|---|
| SPI SCK | GPIO 4 | GC9A01 SCL + SD SCK (shared) |
| SPI MOSI | GPIO 6 | GC9A01 SDA + SD MOSI (shared) |
| SPI MISO | GPIO 3 | SD MISO only (display is write-only) |
| Display CS | GPIO 5 | GC9A01 CS |
| Display DC | GPIO 7 | GC9A01 DC / RS |
| Display RST | GPIO 8 | GC9A01 RES |
| Display BL | GPIO 2 | GC9A01 BLK (backlight) |
| SD CS | GPIO 10 | SD module CS |
| LED data | GPIO 9 | WS2812B DIN (chain: DOUT → DIN between LEDs) |

### GC9A01 round display — 3.3V

```
GC9A01 Pin    ESP32-C3 Pin    Note
─────────────────────────────────────────────────────
VCC           3.3V            NOT 5V
GND           GND
SCL / SCK     GPIO 4          SPI clock (shared with SD)
SDA / MOSI    GPIO 6          SPI data  (shared with SD)
RES           GPIO 8          Reset
DC            GPIO 7          Data / Command select
CS            GPIO 5          Chip select
BLK           GPIO 2          Backlight
```

### Micro SD card module — 3.3V

```
SD Pin        ESP32-C3 Pin    Note
─────────────────────────────────────────────────────
VCC           3.3V            NOT 5V
GND           GND
SCK           GPIO 4          Shared with display
MOSI          GPIO 6          Shared with display
MISO          GPIO 3          SD reads only
CS            GPIO 10         Independent CS
```

Format as FAT32. Up to 32 GB confirmed working.

### WS2812B LEDs — 5V

```
LED Pin       ESP32-C3 Pin    Note
─────────────────────────────────────────────────────
VCC           5V              Requires 5V, not 3.3V
GND           GND
DIN (first)   GPIO 9          300–500 Ω series resistor recommended
```

Chain DOUT → DIN between each LED. A 100 µF cap across 5V/GND helps with power-on current spikes.

---

## Firmware

### Key decisions

**LovyanGFX only — not TFT_eSPI.**
TFT_eSPI uses Arduino's `SPIClass` wrapper which causes a `TG1WDT_SYS_RST` watchdog crash during SPI init on the ESP32-C3. LovyanGFX calls `spi_bus_initialize()` via the ESP-IDF `spi_master` driver directly — no crash, no workaround. Do not switch back.

**NeoPixelBus for LEDs — not FastLED.**
Both the FastLED RMT and I2S backends trigger a guru meditation panic on boot on ESP32-C3 before any serial output appears. `makuna/NeoPixelBus @ ^2.8` with `NeoEsp32Rmt0Ws2812xMethod` works correctly.

**Shared SPI2 bus.**
The display and SD card share SPI2 (SCK = GPIO4, MOSI = GPIO6). `bus_shared=true` and `use_lock=true` in `lgfx_config.h` tell LovyanGFX to call `SPI.endTransaction()` after each display transaction, releasing the bus for the SD library. `sdInit()` must be called after `displayInit()` — LovyanGFX registers `SPI2_HOST` inside `tft.init()` and the SD library reuses that registration.

**SD CS float fix.**
GPIO10 floats at reset. If the SD module asserts itself on the bus before `sdInit()` runs, it corrupts the display init sequence. `displayInit()` drives GPIO10 HIGH before calling `tft.init()`.

### Getting started

```bash
# 1. Open the workspace (PlatformIO needs firmware/ as a root)
# File → Open Workspace from File → muni.code-workspace

# 2. Copy and fill in credentials
cp firmware/include/config.example.h firmware/include/config.h
# Edit: WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP

# 3. Flash
cd firmware
pio run --target upload
pio device monitor --baud 115200
```

Expected serial output on a good boot:
```
=== DeskPet booting ===
[LEDs] Initialised — 3 WS2812B on GPIO9 (NeoPixelBus RMT)
[Display] Initialised
[SD] Mounted — type: SDHC, size: 15193 MB
[Sprites] Cached version: 0.1.2
[Sprites] SD sprites present: yes
[Sprites] Download disabled (SPRITE_DOWNLOAD_ENABLED=0) — using SD card files as-is
[WiFi] Connected! IP: 192.168.x.x
[MQTT] Connected!
=== DeskPet ready ===
[SpritePlayer] Loaded /sprites/muni/neutral.sprite (12 frame(s), looping)
```

---

## Animation System

### Sprite file format

Each `.sprite` file is a flat binary of concatenated frames with no header:

```
Frame 0:  240 × 240 × 2 = 115,200 bytes  (raw little-endian RGB565)
Frame 1:  115,200 bytes
...
Frame N:  115,200 bytes
```

Files live on the SD card at `/sprites/muni/<expression>.sprite`.

### Playback

The sprite player streams frames from SD in 8-row chunks (3,840 bytes at a time) to stay within the ESP32-C3's heap constraints. Each chunk alternates: SD read (SPI2 at 25 MHz) → display push (SPI2 at 40 MHz). With `bus_shared=true` the bus is released between each pair so the two devices never conflict.

Target framerate: **20fps (50ms/frame)**. Actual measured: ~12–16fps on C3 due to 30 SPI bus hand-offs per frame. On the S3 with PSRAM the full frame can be buffered in one read, eliminating the hand-off overhead.

### Expression behaviour

| Expression | Playback mode |
|---|---|
| `neutral` | Loops continuously — permanent idle |
| All others | Plays once through to the last frame, then automatically returns to `neutral` |

Return to neutral is **completion-driven**, not timer-driven. The sprite player signals `spritePlayerFinished()` after the last frame; `expressionTick()` catches it and calls `expressionSet(EXPR_NEUTRAL)`.

If a new expression is triggered while one is already playing, playback switches immediately — no queuing, no wait.

### Programmatic face fallback

If a `.sprite` file is missing for the requested expression, the firmware falls back to a programmatically drawn face (geometric primitives via LovyanGFX). In fallback mode a hold timer (`EXPRESSION_HOLD_MS`, default 8 s) returns Muni to neutral. The programmatic face supports all 7 expressions with blink and breathing animations.

### Expressions

| Name | Sprite behaviour | Programmatic face |
|---|---|---|
| `neutral` | Loops as idle | Calm round eyes, flat mouth, blink + breathe |
| `happy` | Plays once → neutral | Curved eyes, wide smile, soft cheeks |
| `excited` | Plays once → neutral | Wide eyes, big grin, cheeks, sparkles |
| `sad` | Plays once → neutral | Downturned eyes, small frown |
| `surprised` | Plays once → neutral | Large round eyes, open-O mouth |
| `sleepy` | Plays once → neutral | Half-closed eyes, small open mouth |
| `thinking` | Plays once → neutral | One eye narrowed, three dots upper-right |

---

## Connectivity

### Currently working

**MQTT** — primary smart home integration. Broker at `192.168.2.14:1883`.

| Topic | Direction | Payload |
|---|---|---|
| `deskpet/expression` | subscribe | `neutral` / `happy` / `sad` / `surprised` / `sleepy` / `excited` / `thinking` |
| `deskpet/animation` | subscribe | `bounce` / `blink` / `yawn` / `breathe` |
| `deskpet/command` | subscribe | `restart` / `sleep` / `wake` |
| `deskpet/status` | publish | `online` / `offline` (LWT) |
| `deskpet/current_expression` | publish | current expression name |

**USB serial** — open PlatformIO serial monitor at 115200 baud. All subsystem logs are prefixed (`[Display]`, `[SD]`, `[SpritePlayer]`, etc.).

### Planned

**Captive portal** — first-time WiFi setup without reflashing. On first boot with no saved credentials, Muni hosts a WiFi AP + config page.

**WiFi REST API** — `muni.local/expression/<name>` (GET or POST). Any device on the local network can trigger expressions without MQTT. Powers the phone app and Electron companion.

**Expression queue** — depth 3–4, source-agnostic, duplicate-spam protection. The same queue handles MQTT, REST, serial, and Electron inputs so sources can't interfere with each other.

---

## Sprite Server

The sprite server runs locally on the home server (or any Docker host).

```bash
cd sprite-server
docker compose up -d
# Web UI:     http://192.168.2.14:8000
# File server: http://192.168.2.14:8765
```

### Web UI (port 8000)

- Upload a GIF (or PNG/JPEG)
- Set character (`muni`) and expression name (`happy`)
- **Preview** — shows frame count and output size without saving
- **Publish** — converts to RGB565, saves to `sprites/muni/happy.sprite`, bumps manifest version
- **Download** — saves the just-published `.sprite` file to your PC
- **Library** — all published sprites listed as download links

### Conversion

GIF frames → centre-crop to square → resize to 240×240 (Lanczos) → pack as little-endian `uint16_t`:

```python
rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
struct.pack_into('<H', buf, idx, rgb565)
```

### Current download status

Sprite auto-download on boot is **disabled** (`SPRITE_DOWNLOAD_ENABLED 0` in `sprite_manager.cpp`). Muni uses whatever `.sprite` files are already on the SD card.

**Re-enable when the S3 arrives:**
```cpp
// firmware/src/sprite_manager.cpp, line 20:
#define SPRITE_DOWNLOAD_ENABLED 1
```

The download system (manifest fetch, version compare, HTTP streaming to SD) is fully implemented and tested — it just doesn't run on boot until that flag is flipped.

---

## Simulator

Python/pygame desktop simulator — full round-clipped 240×240 display, all expressions, MQTT.

```bash
cd simulator
pip install -r requirements.txt
python simulator.py
```

Keys: `1`–`7` = expressions, `b` = bounce, `y` = yawn, `q` = quit.

---

## Roadmap

### Done

- [x] GC9A01 display with LovyanGFX (40 MHz, shared SPI2, bus_shared)
- [x] All 7 programmatic expressions with blink + breathing animations
- [x] SD card on shared SPI2 bus
- [x] SD sprite streaming — chunked RGB565 playback, 12–16 fps
- [x] Play-once expression sprites → auto-return to looping neutral
- [x] WS2812B LEDs — midnight-blue breathing, NeoPixelBus, non-blocking
- [x] WiFi + auto-reconnect
- [x] MQTT — expressions, animations, commands, LWT
- [x] NVS sprite version cache (Preferences, survives reboot)
- [x] Sprite server — Docker, FastAPI, GIF→RGB565 web UI, download links
- [x] Desktop simulator — pygame, MQTT, round mask
- [x] VS Code multi-root workspace

### Near term

- [ ] Sprite auto-download re-enabled on S3 (flip `SPRITE_DOWNLOAD_ENABLED`)
- [ ] Hand-drawn Muni sprite sheets v0.1 (neutral, happy, sad, surprised, sleepy, excited, thinking)
- [ ] Captive portal WiFi setup
- [ ] WiFi REST API (`muni.local/expression/<name>`)
- [ ] Expression queue (depth 3–4, dedup, source-agnostic)
- [ ] OTA firmware updates
- [ ] Homey flows — arrive home → happy, everyone in bed → sleepy, door opens → surprised

### Companion app (Electron)

Desktop app for Windows/macOS/Linux:

- [ ] Twitch EventSub — new follower, sub, raid → trigger expressions
- [ ] Discord bot — DMs, @mentions → reactions
- [ ] Stream Deck plugin
- [ ] USB serial control (no WiFi required)
- [ ] Expression scheduler (time-of-day moods)

### Phone app

- [ ] Connect to Muni over WiFi REST (`muni.local`)
- [ ] One-tap expression tiles
- [ ] Notification forwarding (calls, messages → surprise/excited)

### Physical

- [ ] 3D printed enclosure — chibi raven body with tiny wings
- [ ] Custom PCB — clean up wiring, add proper 5V → 3.3V regulation
- [ ] Swap ESP32-C3 → S3 N16R8 (ordered)

### Stage 2 — data visualisation

- [ ] Print progress ring — cyan arc 0–100% on `deskpet/printer_progress`, excited at 100%
- [ ] Temperature arc — colour-coded room/outdoor temperature
- [ ] General data ring — any 0–100% MQTT value

---

## Notes

- `firmware/include/config.h` is gitignored — never commit it. Use `config.example.h` as the template.
- `platformio.ini` uses the `min_spiffs` partition scheme (~190 KB LittleFS). Sprite files live on the SD card, so this partition only needs to hold NVS overhead — it's fine.
- The programmatic face is always the ground truth. Sprites are cosmetic. A missing file or SD failure changes nothing functionally.
- LED breathing period is 4 seconds, matching the programmatic face's breathing sine wave, so the room light and Muni's eyes pulse in sync.

---

*Huginn thinks. Muninn remembers. Muni watches your desk.*
