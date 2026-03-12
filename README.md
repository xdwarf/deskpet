# DeskPet

> An interactive animated desk companion with smart home integration and a Danish-language voice AI assistant.

DeskPet sits on your desk and reacts to your smart home — showing expressions when you arrive home, when it's raining, when everyone goes to bed, and more. Eventually it will also act as a conversational AI assistant in Danish, responding through your Google Nest speakers.

---

## Table of Contents

- [Project Vision](#project-vision)
- [Hardware](#hardware)
- [Architecture](#architecture)
- [Build Stages](#build-stages)
- [Wiring — ESP32-C3 to GC9A01](#wiring--esp32-c3-to-gc9a01)
- [MQTT Topic Structure](#mqtt-topic-structure)
- [Getting Started](#getting-started)
- [Repository Structure](#repository-structure)
- [Notes & Contributing](#notes--contributing)

---

## Project Vision

DeskPet is a physical companion device that bridges your smart home and your desk. It has:

- A **round animated face** on a GC9A01 240×240 display, showing expressions that react to real-world events
- **MQTT integration** so your Homey smart home hub can trigger moods and reactions
- A **voice assistant** (built on a Raspberry Pi 3B+) that listens for a wake word, understands Danish, queries an AI, and speaks back through your Google Nest speaker
- A **3D-printed body** to give it a physical character presence on your desk

The character's name is **Neo**.

---

## Hardware

| Component | Role | Notes |
|---|---|---|
| ESP32-C3 Mini | Main controller, display driver | WiFi built-in, small form factor |
| GC9A01 | 240×240 round SPI display | Already owned |
| Raspberry Pi 3B+ | Voice assistant brain | Runs Python wake word + AI pipeline |
| USB Webcam | Microphone input | Used only for mic, not camera |
| Google Nest Speaker | Audio output | pychromecast on the Pi casts audio directly |
| 3D Printer | Physical enclosure | Design TBD in Stage 4 |

**Home Server (already running on 192.168.2.14):**
- Docker on Ubuntu on Proxmox
- Mosquitto MQTT broker — port 1883
- Homey Self-Hosted smart home hub

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        HOME NETWORK                             │
│                                                                 │
│  ┌──────────────┐     MQTT      ┌─────────────────────────┐    │
│  │    Homey     │──────────────▶│   Mosquitto MQTT Broker │    │
│  │ (Smart Home) │               │    192.168.2.14:1883    │    │
│  └──────────────┘               └────────────┬────────────┘    │
│         ▲                                    │                  │
│         │ Events                    MQTT     │ MQTT             │
│  (door, presence,              (expressions) │ (status)         │
│   weather, etc.)                             │                  │
│                                ┌─────────────▼──────────┐      │
│                                │      ESP32-C3 Mini      │      │
│                                │  ┌──────────────────┐  │      │
│                                │  │  GC9A01 Display  │  │      │
│                                │  │   (Neo's face)   │  │      │
│                                │  └──────────────────┘  │      │
│                                └────────────────────────┘      │
│                                                                 │
│  ┌──────────────────────┐   HTTP    ┌──────────────────────┐   │
│  │   Raspberry Pi 3B+   │──────────▶│  neo-brain (Docker)  │   │
│  │                      │  POST WAV │  192.168.2.14:8000   │   │
│  │  Wake word detect    │◀──────────│                      │   │
│  │  Audio capture       │  text     │  POST /transcribe    │   │
│  │  gTTS (Danish)       │──────────▶│    Vosk STT (Danish) │   │
│  │  pychromecast cast   │  POST txt │                      │   │
│  │  MQTT status         │◀──────────│  POST /ask           │   │
│  └──────────┬───────────┘  response │    Gemini AI         │   │
│             │                       └──────────────────────┘   │
│             │ pychromecast (direct)                             │
│             ▼                                                   │
│   ┌─────────────────┐                                           │
│   │  Google Nest    │                                           │
│   │ 192.168.2.172   │                                           │
│   └─────────────────┘                                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Build Stages

### Stage 1 — Desk Pet Display (ESP32-C3 + GC9A01)
*Goal: Neo's face is alive on the desk, reacting to smart home events.*

- [x] Project repository and structure set up
- [x] PlatformIO firmware compiles and flashes to ESP32-C3
- [x] GC9A01 display initialises and renders graphics (LovyanGFX, 40 MHz, invert=true)
- [x] Idle animations: blinking, breathing effect
- [x] Expression system: neutral, happy, sad, surprised, sleepy, excited, thinking
- [x] EXCITED expression visually distinct from HAPPY (wide eyes, bigger grin, sparkles)
- [x] WiFi connection with reconnect logic
- [x] MQTT client connects to broker at 192.168.2.14:1883
- [x] Expression changes on MQTT message (`deskpet/expression`)
- [ ] Trigger: someone arrives home → happy bouncy expression
- [ ] Trigger: everyone in bed → sleepy expression
- [ ] Trigger: door/window opens → surprised expression
- [ ] Trigger: good morning → yawning/waking-up animation
- [ ] Trigger: rain forecast → sad expression

### Stage 2 — Voice Assistant (Pi + Docker server)
*Goal: Say "Hey Neo", ask something in Danish, hear an answer on the Nest speaker.*

**Pi (voice-assistant/):**
- [x] Python project structure and all modules written
- [x] Wake word detection (openwakeword, placeholder: hey_jarvis)
- [x] Microphone input via USB webcam (pyaudio)
- [x] Records audio as WAV, POSTs to neo-brain server for STT
- [x] POSTs transcript to neo-brain server for AI response
- [x] Text-to-speech response in Danish (gTTS)
- [x] Cast audio directly to Google Nest via pychromecast (192.168.2.172)
- [x] ESP32 reacts: thinking face while processing, happy when responding
- [x] MQTT handshake between Pi and ESP32 for state sync

**Docker server (vosk-server/):**
- [x] FastAPI server with `/transcribe` (Vosk) and `/ask` (Gemini) endpoints
- [x] Dockerfile + docker-compose.yaml ready
- [ ] Deploy to Docker host and download Danish Vosk model
- [ ] End-to-end test of full pipeline on physical hardware
- [ ] Train or source custom "Hey Neo" wake word model

### Stage 3 — Smart Home Deep Integration
*Goal: Neo is a true smart home dashboard personality.*

- [ ] Homey MQTT flows trigger all expressions
- [ ] Weather mood based on forecast data
- [ ] Presence detection integration
- [ ] Door/window sensor reactions
- [ ] Temperature alert reactions
- [ ] Optional: time/temperature info display on screen

### Stage 4 — Physical Body
*Goal: Neo has a body and lives on the desk.*

- [ ] Character design sketched
- [ ] 3D model designed (enclosure for ESP32 + display)
- [ ] First print test
- [ ] Final print and assembly
- [ ] Cable management inside body

### Stage 4.5 — 3D Printer Integration
*Goal: Neo watches over your prints and celebrates when they finish.*

Neo shows a live print progress ring on the display while the printer is running.
A circular arc grows around the outside of the face from 0 % to 100 % as the
print progresses. When the print completes, Neo triggers the excited expression
and animation to celebrate.

**How it works:**
- Your slicer or print server (OrcaSlicer, OctoPrint, Bambu, etc.) publishes
  print progress to `deskpet/printer_progress` as a value `0`–`100`
- The ESP32 draws a progress ring over the current face expression in real time
- At `100` the ring completes and Neo switches to the excited expression

**Progress ring appearance:**
- Drawn as a thick arc around the outer edge of the round display
- Colour: cyan (`#00FFFF`) — matches the accent colour used for sparkles
- Arc grows clockwise from the top (12 o'clock position), 0 % → full circle
- The underlying face expression continues to animate behind the ring

**MQTT trigger:**
```
Topic:    deskpet/printer_progress
Payload:  0–100  (integer, percentage complete)
Example:  mosquitto_pub -h 192.168.2.14 -t deskpet/printer_progress -m 47
```

- [ ] Subscribe to `deskpet/printer_progress` in MQTT client
- [ ] Add `printerProgress` state (0–100, -1 = idle) to expressions system
- [ ] Draw progress ring arc in `drawFace()` when `printerProgress >= 0`
- [ ] Trigger excited expression + bounce animation at 100 %
- [ ] Auto-clear progress ring after celebration (return to normal idle)
- [ ] Test with OrcaSlicer / OctoPrint MQTT plugin
- [ ] Add Homey flow to bridge printer events to `deskpet/printer_progress`

---

## Wiring — ESP32-C3 to GC9A01

The GC9A01 uses SPI. Wire it to the ESP32-C3 Mini as follows:

```
GC9A01 Pin    →    ESP32-C3 Mini Pin    Notes
──────────────────────────────────────────────────────────────
VCC           →    3.3V                 Power (NOT 5V!)
GND           →    GND                 Ground
SCL / SCK     →    GPIO4               SPI Clock
SDA / MOSI    →    GPIO6               SPI Data (MOSI)
RES / RST     →    GPIO8               Reset
DC            →    GPIO7               Data/Command select
CS            →    GPIO5               Chip Select (active LOW)
BLK / BL      →    GPIO2 (or 3.3V)     Backlight — GPIO allows PWM dimming
```

> **Note:** The ESP32-C3 Mini has limited GPIO. The pins above avoid boot-strapping
> pins and the USB serial pins (GPIO19/20). Double-check your specific board pinout
> before wiring — some ESP32-C3 Mini variants differ slightly.

**SPI bus used:** SPI2 (the hardware SPI peripheral on ESP32-C3)

---

## MQTT Topic Structure

All topics use the prefix `deskpet/`. The broker runs at `192.168.2.14:1883`.

### ESP32 subscribes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `happy` / `sad` / `surprised` / `sleepy` / `excited` / `thinking` / `neutral` | Set Neo's expression directly |
| `deskpet/animation` | `bounce` / `blink` / `yawn` / `breathe` | Trigger a one-shot animation |
| `deskpet/command` | `restart` / `sleep` / `wake` | System commands |
| `deskpet/printer_progress` | `0`–`100` | 3D print progress — draws a ring around the face; `100` triggers excited |

### ESP32 publishes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/status` | `online` / `offline` | LWT (Last Will and Testament) for connection state |
| `deskpet/current_expression` | expression name | Confirms currently displayed expression |

### Voice assistant publishes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `thinking` | When processing a voice query |
| `deskpet/expression` | `happy` | When delivering a response |
| `deskpet/voice/status` | `listening` / `processing` / `speaking` / `idle` | Voice pipeline state |

### Homey publishes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `happy` | Someone arrived home |
| `deskpet/expression` | `sleepy` | Everyone in bed |
| `deskpet/expression` | `surprised` | Door/window opened |
| `deskpet/animation` | `yawn` | Good morning trigger |
| `deskpet/expression` | `sad` | Rain forecast |

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) — installed as VS Code extension or CLI
- Python 3.10+ on the Raspberry Pi
- MQTT broker running at `192.168.2.14:1883`
- WiFi credentials for your network

### Stage 1: Flash the ESP32-C3

```bash
# Clone the repo
git clone <your-repo-url>
cd deskpet

# Open the firmware folder in VS Code with PlatformIO, or use CLI:
cd firmware

# Copy and edit the config file
cp include/config.example.h include/config.h
# Edit config.h with your WiFi SSID, password, and MQTT broker IP

# Build and flash
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Stage 2a: Start the neo-brain server (Docker host)

```bash
cd vosk-server

# Download the Danish Vosk model (~48 MB, one-time setup)
mkdir -p models
wget -P models https://alphacephei.com/vosk/models/vosk-model-small-da-0.22.zip
unzip models/vosk-model-small-da-0.22.zip -d models/
rm models/vosk-model-small-da-0.22.zip

# Add your Gemini API key
cp .env.example .env
# Edit .env with your key

# Start the server
docker compose up -d

# Verify it's running
curl http://192.168.2.14:8000/health
```

### Stage 2b: Set up the Voice Assistant (Raspberry Pi)

```bash
cd voice-assistant

# Create a Python virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies (no Whisper or Gemini SDK — server handles those)
pip install -r requirements.txt

# Copy and edit config
cp config/config.example.yaml config/config.yaml
# Edit config.yaml: set serve_host to the Pi's LAN IP (run hostname -I)

# Run
python src/main.py
```

---

## Repository Structure

```
deskpet/
├── firmware/                   # ESP32-C3 PlatformIO project
│   ├── src/
│   │   ├── main.cpp            # Entry point — setup() + loop()
│   │   ├── display.cpp/.h      # GC9A01 display driver wrapper
│   │   ├── expressions.cpp/.h  # Face expressions and animations
│   │   ├── wifi_manager.cpp/.h # WiFi connection + reconnect logic
│   │   └── mqtt_client.cpp/.h  # MQTT subscribe/publish logic
│   ├── include/
│   │   ├── config.h            # WiFi/MQTT credentials (gitignored)
│   │   └── config.example.h    # Template — copy to config.h
│   ├── lib/                    # Local libraries (if any)
│   ├── test/                   # Unit tests (PlatformIO test runner)
│   └── platformio.ini          # PlatformIO project configuration
│
├── voice-assistant/            # Raspberry Pi Python project
│   ├── src/
│   │   ├── main.py             # Entry point
│   │   ├── wake_word.py        # Wake word detection
│   │   ├── stt.py              # Speech-to-text (Danish)
│   │   ├── ai_client.py        # Gemini API integration
│   │   ├── tts.py              # Text-to-speech (Danish)
│   │   └── mqtt_bridge.py      # MQTT publish/subscribe
│   ├── config/
│   │   ├── config.yaml         # Live config (gitignored)
│   │   └── config.example.yaml # Template — copy to config.yaml
│   └── requirements.txt        # Python dependencies
│
├── vosk-server/                # Docker server — Vosk STT + Gemini AI backend
│   ├── main.py                 # FastAPI app (/transcribe + /ask endpoints)
│   ├── Dockerfile              # Python 3.11-slim image
│   ├── docker-compose.yaml     # Service definition + model volume mount
│   ├── requirements.txt        # Server Python dependencies
│   └── .env.example            # Template — copy to .env with Gemini API key
│
├── homey/                      # Homey MQTT flow docs and examples
│   └── flows.md                # Flow documentation
│
├── 3d-models/                  # STL/3MF files for the physical body
│
├── docs/                       # Additional documentation
│   └── hardware-notes.md       # Hardware-specific notes
│
└── README.md                   # This file
```

---

## Notes & Contributing

This is a personal DIY project. Notes on key decisions:

- **ESP32-C3 over ESP32-S3:** The C3 Mini is smaller and cheaper. It lacks the dual-core of the S3 but for display driving + MQTT it is more than sufficient for Stage 1.
- **GC9A01 driver:** We use `LovyanGFX`. TFT_eSPI was the original choice but its Arduino SPIClass wrapper causes a TG1WDT watchdog crash during SPI initialisation on ESP32-C3. LovyanGFX initialises SPI2 via the ESP-IDF `spi_master` driver directly, which works correctly.
- **Split voice pipeline:** The Pi handles only audio I/O, wake word, TTS, and casting. The Docker server (`vosk-server/`) handles Vosk STT and Gemini AI. This keeps the Pi lightweight and API keys off the device.
- **STT:** Vosk with the `vosk-model-small-da-0.22` Danish model running in Docker. The Pi records 16 kHz mono WAV and POSTs it to `POST /transcribe` on the server.
- **AI backend:** Google Gemini 1.5 Flash called from the Docker server's `POST /ask` endpoint. Pi only sees the response text — no Gemini API key needed on the Pi.
- **Audio casting:** pychromecast connects directly to the Nest speaker at `192.168.2.172:8009`. The Pi serves the generated MP3 over a local HTTP server so the Nest can fetch it by URL — no AirCast needed.
- **No OTA in Stage 1:** Over-the-air updates will be added once the base firmware is stable.
- **Danish language:** All voice pipeline components are chosen and configured for Danish (`da-DK`).
- **Security:** WiFi credentials and API keys are never committed — always use `config.h` / `config.yaml` which are gitignored.

---

*Neo is watching. Neo is waiting. Neo will tell you when it's going to rain.*
