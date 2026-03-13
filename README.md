# DeskPet

> Muni (Muninn) — Odin's raven, rendered in chibi Norse/Viking style, watching over your desk.

Muni is a small animated raven who lives on a round 240×240 display and reacts to your smart home via MQTT. Happy when you arrive home, sleepy when everyone's in bed, surprised when a door opens. Drawn in a chibi Viking style — round head, tiny wings, a hint of runic mischief. Runs entirely on an ESP32-C3 SuperMini.

---

## Repository Structure

```
deskpet/
├── firmware/        # ESP32-C3 PlatformIO project (C++ / Arduino)
├── sprite-server/   # Docker nginx service — serves sprite sheets over HTTP
├── simulator/       # Python desktop app — simulates the round display on PC
└── README.md
```

---

## Hardware

| Component | Notes |
|---|---|
| ESP32-C3 SuperMini | Main controller — WiFi built-in, tiny form factor |
| GC9A01 | 240×240 round SPI display |

**Home server (already running at 192.168.2.14):**
- Mosquitto MQTT broker — port 1883
- Docker host — runs the sprite server

---

## Wiring — ESP32-C3 to GC9A01

The GC9A01 uses SPI. Wire it to the ESP32-C3 SuperMini as follows:

```
GC9A01 Pin    →    ESP32-C3 Pin    Notes
──────────────────────────────────────────────────────────────
VCC           →    3.3V            Power (NOT 5V!)
GND           →    GND             Ground
SCL / SCK     →    GPIO4           SPI Clock
SDA / MOSI    →    GPIO6           SPI Data (MOSI)
RES / RST     →    GPIO8           Reset
DC            →    GPIO7           Data/Command select
CS            →    GPIO5           Chip Select (active LOW)
BLK / BL      →    GPIO2           Backlight — GPIO allows PWM dimming
```

> Pins above avoid boot-strapping pins and the USB serial pins (GPIO19/20).
> Double-check your specific board pinout — SuperMini variants can differ slightly.

**SPI bus:** SPI2 (hardware SPI peripheral on ESP32-C3)

---

## Getting Started

### Flash the firmware

**Prerequisites:** [PlatformIO](https://platformio.org/) (VS Code extension or CLI), MQTT broker on your network.

```bash
cd firmware

# Copy and fill in your credentials
cp include/config.example.h include/config.h
# Edit config.h: set WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP

# Build and flash
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Run the simulator (no hardware needed)

```bash
cd simulator
pip install -r requirements.txt
python simulator.py
# Keys: 1-7 = expressions, b = bounce, y = yawn, q = quit
```

### Deploy the sprite server

```bash
cd sprite-server

# Ensure the external Docker network exists (one-time setup on the Docker host)
docker network create proxy

# Start the server
docker compose up -d
```

Then configure Nginx Proxy Manager to route `sprites.mael.dk` to the `deskpet-sprite-server` container on port 80.

---

## MQTT Topics

All topics use the prefix `deskpet/`. Broker: `192.168.2.14:1883`.

### ESP32 subscribes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `happy` / `sad` / `surprised` / `sleepy` / `excited` / `thinking` / `neutral` | Set Muni's expression |
| `deskpet/animation` | `bounce` / `blink` / `yawn` / `breathe` | Trigger a one-shot animation |
| `deskpet/command` | `restart` / `sleep` / `wake` | System commands |
| `deskpet/printer_progress` | `0`-`100` | Print progress ring — `100` triggers excited |

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
2. Compares the `"version"` field against the version stored in NVS (non-volatile storage — survives reboots)
3. If newer: downloads each expression's `.sprite` file to LittleFS on-flash storage
4. If same version or server unreachable: uses cached files from LittleFS
5. If no cache exists at all: falls back silently to the programmatic face

### Sprite file format

Each `.sprite` file is a raw RGB565 bitmap: `240 x 240 x 2 bytes = 115,200 bytes`.
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

Bump `"version"` to trigger a re-download on all devices.

### Adding sprites

Place `.sprite` files in `sprite-server/sprites/muni/`, bump the version in `manifest.json`, and redeploy. Devices will download the update on next boot.

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
- [x] Sprite server with nginx + Docker + manifest
- [x] Firmware sprite cache (NVS version check, LittleFS download)
- [x] Desktop simulator (pygame, all expressions, MQTT)
- [ ] Homey flows: someone arrives → happy, everyone in bed → sleepy
- [ ] Door/window opens → surprised
- [ ] Good morning → yawn animation
- [ ] Rain forecast → sad
- [ ] Hand-drawn Muni sprite sheets v0.1

### Stage 2 — Progress Ring & Data Dials

Muni's round display is perfect for circular data visualisations drawn around the outside of his face.

- [ ] **Print progress ring** — arc grows clockwise 0-100% on `deskpet/printer_progress`; triggers excited at 100%
- [ ] **Temperature dial** — colour-coded arc showing current room/outdoor temperature
- [ ] **General data ring** — generic 0-100% arc driven by any MQTT value

### Stage 3 — Physical Body

- [ ] Character design sketched (chibi raven with tiny wings and runic details)
- [ ] 3D model designed (enclosure for ESP32-C3 SuperMini + GC9A01)
- [ ] First print test
- [ ] Final print and assembly

### Stage 3.5 — 3D Printer Integration

Muni watches over your prints and celebrates when they finish.

- [ ] Subscribe to `deskpet/printer_progress` (`0`-`100`)
- [ ] Draw cyan progress ring arc around face in real time
- [ ] Trigger excited expression + bounce at 100%
- [ ] Auto-clear ring after celebration
- [ ] Add Homey / OctoPrint / OrcaSlicer flow to publish progress

---

## Notes

- **LovyanGFX over TFT_eSPI:** TFT_eSPI's Arduino `SPIClass` wrapper triggers a `TG1WDT_SYS_RST` watchdog crash during SPI initialisation on the ESP32-C3. LovyanGFX initialises SPI2 directly via the ESP-IDF `spi_bus_initialize()` call, which works correctly. Don't switch back.
- **Credentials:** WiFi SSID/password and MQTT host are in `include/config.h`, which is gitignored. Never commit it — use `config.example.h` as the template.
- **LittleFS partition:** `platformio.ini` uses the `min_spiffs` partition scheme, giving ~190 KB for sprite files alongside the app. If you add more sprites, switch to a custom partition table.
- **Sprite fallback:** The programmatic face is always the ground truth. Sprites are purely cosmetic — if the download fails or LittleFS is full, nothing breaks.

---

*Huginn thinks. Muninn remembers. Muni watches your desk.*
