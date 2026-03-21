# DeskPet

> Muni (Muninn) — Odin's raven, rendered in chibi Norse/Viking style, watching over your desk.

Muni is a small animated raven who lives on a round 240×240 display and reacts to your smart home via MQTT. Happy when you arrive home, sleepy when everyone's in bed, surprised when a door opens. Drawn in a chibi Viking style — round head, tiny wings, a hint of runic mischief. Runs entirely on an ESP32-C3 SuperMini.

---

## Repository Structure

```
deskpet/
├── firmware/            # ESP32-C3 PlatformIO project (C++ / Arduino)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── display.cpp/.h       # LovyanGFX display driver wrapper
│   │   ├── expressions.cpp/.h   # Programmatic face animations
│   │   ├── wifi_manager.cpp/.h  # WiFi connection + reconnect
│   │   ├── mqtt_client.cpp/.h   # MQTT subscribe/publish
│   │   ├── sprite_manager.cpp/.h # NVS version check + LittleFS cache
│   │   ├── sd_card.cpp/.h       # SD card mount on shared SPI bus
│   │   └── leds.cpp/.h          # WS2812B breathing effect
│   ├── include/
│   │   ├── config.example.h     # Template — copy to config.h
│   │   ├── config.h             # Credentials + pin defines (gitignored)
│   │   └── lgfx_config.h        # LovyanGFX panel + bus configuration
│   └── platformio.ini
├── sprite-server/       # Docker nginx service — serves sprite sheets over HTTP
├── simulator/           # Python desktop app — simulates the round display on PC
├── muni.code-workspace  # VS Code multi-root workspace (open this)
└── README.md
```

---

## Hardware

| Component | Role | Notes |
|---|---|---|
| ESP32-C3 SuperMini | Main controller | WiFi built-in, tiny form factor |
| GC9A01 | 240×240 round SPI display | SPI2, 40 MHz |
| Micro SD card module | Animation frame storage | Shares SPI2 with display |
| 3× WS2812B LEDs | Ambient light / status | GPIO9, 5V, breathing midnight blue |

**Home server (running at 192.168.2.14):**
- Mosquitto MQTT broker — port 1883
- Docker host — runs the sprite server

---

## Wiring

### SPI Bus — shared by display and SD card

Both the GC9A01 display and the SD card module share SPI2 on the ESP32-C3.
They use the same SCK and MOSI lines, with separate CS pins and their own MISO on the SD side.

```
Signal        ESP32-C3 Pin    Connected to
─────────────────────────────────────────────────────────────
SCK           GPIO4           GC9A01 SCL  +  SD SCK
MOSI          GPIO6           GC9A01 SDA  +  SD MOSI
MISO          GPIO3           SD MISO only (display is write-only)
```

### GC9A01 Round Display (3.3V)

```
GC9A01 Pin    ESP32-C3 Pin    Notes
──────────────────────────────────────────────────────────────
VCC           3.3V            Power — NOT 5V
GND           GND
SCL / SCK     GPIO4           SPI clock (shared)
SDA / MOSI    GPIO6           SPI data (shared)
RES / RST     GPIO8           Reset
DC            GPIO7           Data / Command select
CS            GPIO5           Chip select (active LOW)
BLK / BL      GPIO2           Backlight (PWM dimming)
```

### Micro SD Card Module (3.3V)

```
SD Pin        ESP32-C3 Pin    Notes
──────────────────────────────────────────────────────────────
VCC           3.3V            Power — NOT 5V
GND           GND
SCK           GPIO4           SPI clock (shared with display)
MOSI          GPIO6           SPI data (shared with display)
MISO          GPIO3           SD reads only
CS            GPIO10          Chip select — independent
```

> Format the SD card as FAT32. Cards up to 32 GB work reliably.
> Mount result is logged to serial on boot: `[SD] Mounted — type: SDHC, size: NNNN MB`

### WS2812B RGB LEDs (5V)

Three LEDs chained in series. Power from 5V, data signal from GPIO9.

```
LED Pin       ESP32-C3 Pin    Notes
──────────────────────────────────────────────────────────────
VCC           5V              WS2812B requires 5V (NOT 3.3V)
GND           GND
DIN (first)   GPIO9           Data in — chain the DOUT → DIN between LEDs
```

> A 300–500 Ω resistor in series on the data line is recommended to protect
> against ringing. A 100 µF capacitor across the 5V/GND supply helps with
> current spikes on power-on.

---

## Getting Started

### 1. Open the workspace

Open `muni.code-workspace` in VS Code (`File → Open Workspace from File…`).
This gives you two roots: the repo root (for Claude Code and git) and `firmware/`
(so PlatformIO finds `platformio.ini` directly and shows the build toolbar).

### 2. Configure credentials

```bash
cd firmware
cp include/config.example.h include/config.h
# Edit config.h — set WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP
```

### 3. Flash the firmware

Via PlatformIO toolbar in VS Code, or CLI:

```bash
cd firmware
pio run --target upload
pio device monitor --baud 115200
```

Expected serial output on a successful boot:
```
=== DeskPet booting ===
[LEDs] Initialised — 3 WS2812B on GPIO9
[Display] Calling tft.init()...
[Display] Initialised
[SD] Initialising...
[SD] Mounted — type: SDHC, size: 15193 MB
[WiFi] Connecting to SSID: ...
[WiFi] Connected! IP address: 192.168.x.x
[MQTT] Connected!
[Sprites] Fetching manifest...
=== DeskPet ready ===
```

### 4. Run the simulator (no hardware needed)

```bash
cd simulator
pip install -r requirements.txt
python simulator.py
# Keys: 1-7 = expressions, b = bounce, y = yawn, q = quit
```

### 5. Deploy the sprite server

```bash
cd sprite-server
docker network create proxy   # one-time on the Docker host
docker compose up -d
```

Configure Nginx Proxy Manager to route `sprites.mael.dk` → `deskpet-sprite-server:80`.

---

## MQTT Topics

All topics use the prefix `deskpet/`. Broker: `192.168.2.14:1883`.

### ESP32 subscribes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `happy` / `sad` / `surprised` / `sleepy` / `excited` / `thinking` / `neutral` | Set Muni's expression |
| `deskpet/animation` | `bounce` / `blink` / `yawn` / `breathe` | Trigger a one-shot animation |
| `deskpet/command` | `restart` / `sleep` / `wake` | System commands |
| `deskpet/printer_progress` | `0`–`100` | Print progress ring — `100` triggers excited |

### ESP32 publishes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/status` | `online` / `offline` | LWT — connection state |
| `deskpet/current_expression` | expression name | Confirms currently displayed expression |

---

## Expressions

| Expression | Description |
|---|---|
| `neutral` | Calm open eyes — default idle state |
| `happy` | Curved eyes, wide smile |
| `excited` | Wide eyes, bigger grin, sparkles |
| `sad` | Downturned eyes, small frown |
| `surprised` | Large round eyes, open mouth |
| `sleepy` | Half-closed eyes, slow breathing |
| `thinking` | One eye narrowed, looking up |

---

## Sprite System — sprites.mael.dk

Muni's face starts as programmatic geometric drawing (always works, no files needed). The sprite system overlays hand-drawn or AI-generated artwork when available.

### How it works

On every boot (after WiFi connects), the ESP32:

1. Fetches `http://sprites.mael.dk/manifest.json`
2. Compares `"version"` against the version stored in NVS (survives reboots)
3. If newer: downloads each expression's `.sprite` file to LittleFS
4. If same version or server unreachable: uses cached LittleFS files
5. If no cache at all: falls back silently to the programmatic face

### Sprite file format

Raw RGB565 bitmap: `240 × 240 × 2 bytes = 115,200 bytes`.
Stored in LittleFS at `/sprites/muni/<expression>.sprite`.

### manifest.json

```json
{
  "version": "0.0.1",
  "characters": ["muni", "odin"],
  "expressions": ["neutral", "happy", "sad", "surprised", "sleepy", "excited", "thinking"],
  "updated": "2026-03-13"
}
```

Bump `"version"` to trigger a re-download on all devices on next boot.

### Adding sprites

Place `.sprite` files in `sprite-server/sprites/muni/`, bump the version in `manifest.json`, redeploy. Devices update on next boot.

---

## Roadmap

### Stage 1 — Animated Face (current)

- [x] PlatformIO firmware compiles and flashes to ESP32-C3
- [x] GC9A01 initialises with LovyanGFX (40 MHz, invert=true)
- [x] Idle animations: blinking, breathing effect
- [x] Expression system: all 7 expressions
- [x] EXCITED visually distinct from HAPPY (wide eyes, bigger grin, sparkles)
- [x] WiFi connection with reconnect logic
- [x] MQTT client connects and subscribes
- [x] Expression changes on `deskpet/expression` message
- [x] Sprite server (nginx + Docker + manifest.json)
- [x] Firmware sprite cache (NVS version check, LittleFS download, graceful fallback)
- [x] Desktop simulator (pygame, all expressions, MQTT, round clip mask)
- [x] SD card mounted on shared SPI2 bus (FAT32, ready for animation frames)
- [x] WS2812B LEDs — slow midnight-blue breathing, non-blocking
- [x] VS Code multi-root workspace (`muni.code-workspace`)
- [ ] Homey flows: someone arrives → happy, everyone in bed → sleepy
- [ ] Door/window opens → surprised
- [ ] Good morning → yawn animation
- [ ] Rain forecast → sad
- [ ] Hand-drawn Muni sprite sheets v0.1
- [ ] RGB565 animation frames on SD card streamed to display

### Stage 2 — Progress Ring & Data Dials

Muni's round display is perfect for circular data visualisations around the outside of his face.

- [ ] **Print progress ring** — arc grows clockwise 0–100% on `deskpet/printer_progress`; triggers excited at 100%
- [ ] **Temperature dial** — colour-coded arc showing current room/outdoor temperature
- [ ] **General data ring** — generic 0–100% arc driven by any MQTT value

### Stage 3 — Physical Body

- [ ] Character design sketched (chibi raven with tiny wings and runic details)
- [ ] 3D model designed (enclosure for ESP32-C3 SuperMini + GC9A01 + LED ring)
- [ ] First print test
- [ ] Final print and assembly

### Stage 3.5 — 3D Printer Integration

Muni watches over your prints and celebrates when they finish.

- [ ] Subscribe to `deskpet/printer_progress` (`0`–`100`)
- [ ] Draw cyan progress ring arc around face in real time
- [ ] Trigger excited expression + bounce at 100%
- [ ] Auto-clear ring after celebration
- [ ] Add Homey / OctoPrint / OrcaSlicer flow to publish progress

---

## Notes

- **LovyanGFX over TFT_eSPI:** TFT_eSPI's `SPIClass` wrapper triggers a `TG1WDT_SYS_RST` watchdog crash during SPI init on the ESP32-C3. LovyanGFX calls `spi_bus_initialize()` via ESP-IDF directly — no crash. Don't switch back.
- **Shared SPI bus:** `bus_shared=true` in `lgfx_config.h` tells LovyanGFX to release SPI2 between display transactions. The SD library's `SPIClass(FSPI)` instance reuses the same SPI2_HOST registration and takes the bus when the display is idle.
- **SD init order:** `sdInit()` must be called after `displayInit()`. LovyanGFX registers the SPI2 bus host inside `tft.init()` — the SD library depends on that registration already existing.
- **LED timing:** The WS2812B breathing uses FastLED's `sin8()` (integer sine, no floats) at a 20 ms tick interval. The 4-second period matches the display's breathing animation so the room light and Muni's face pulse in sync.
- **Credentials:** `include/config.h` is gitignored. Never commit it — use `config.example.h` as the template.
- **LittleFS partition:** `platformio.ini` uses the `min_spiffs` scheme (~190 KB for sprites). For larger sprite sets, switch to a custom partition table.
- **Sprite fallback:** The programmatic face is always the ground truth. Sprites are cosmetic only — a failed download or full filesystem changes nothing visible.

---

*Huginn thinks. Muninn remembers. Muni watches your desk.*
