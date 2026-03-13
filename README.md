# DeskPet

> Kobo Mochi — an animated desk companion that reacts to your smart home.

Kobo sits on your desk and reacts to real-world events via MQTT: happy when you arrive home, sleepy when everyone's in bed, surprised when a door opens. He lives on a round 240×240 display and runs entirely on an ESP32-C3 SuperMini.

---

## Hardware

| Component | Notes |
|---|---|
| ESP32-C3 SuperMini | Main controller — WiFi built-in, tiny form factor |
| GC9A01 | 240×240 round SPI display |

**Home server (already running at 192.168.2.14):**
- Mosquitto MQTT broker — port 1883

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

**Prerequisites:** [PlatformIO](https://platformio.org/) (VS Code extension or CLI), MQTT broker on your network.

```bash
# Clone and enter the firmware folder
git clone <your-repo-url>
cd deskpet/firmware

# Copy and fill in your credentials
cp include/config.example.h include/config.h
# Edit config.h: set WIFI_SSID, WIFI_PASSWORD, MQTT_HOST

# Build and flash
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## MQTT Topics

All topics use the prefix `deskpet/`. Broker: `192.168.2.14:1883`.

### ESP32 subscribes to:

| Topic | Payload | Description |
|---|---|---|
| `deskpet/expression` | `happy` / `sad` / `surprised` / `sleepy` / `excited` / `thinking` / `neutral` | Set Kobo's expression |
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
- [ ] Homey flows: someone arrives → happy, everyone in bed → sleepy
- [ ] Door/window opens → surprised
- [ ] Good morning → yawn animation
- [ ] Rain forecast → sad

### Stage 2 — Progress Ring & Data Dials

Kobo's round display is perfect for circular data visualisations drawn around the outside of his face.

- [ ] **Print progress ring** — arc grows clockwise 0→100% on `deskpet/printer_progress`; triggers excited at 100%
- [ ] **Temperature dial** — colour-coded arc showing current room/outdoor temperature
- [ ] **General data ring** — generic 0–100% arc driven by any MQTT value for custom automations
- [ ] Ring and face animate simultaneously (ring is drawn over the idle face)

### Stage 3 — Physical Body

- [ ] Character design sketched
- [ ] 3D model designed (enclosure for ESP32-C3 SuperMini + GC9A01)
- [ ] First print test
- [ ] Final print and assembly

### Stage 3.5 — 3D Printer Integration

Kobo watches over your prints and celebrates when they finish.

- [ ] Subscribe to `deskpet/printer_progress` (`0`–`100`)
- [ ] Draw cyan progress ring arc around face in real time
- [ ] Trigger excited expression + bounce at 100%
- [ ] Auto-clear ring after celebration
- [ ] Add Homey / OctoPrint / OrcaSlicer flow to publish progress

---

## Notes

- **LovyanGFX over TFT_eSPI:** TFT_eSPI's Arduino `SPIClass` wrapper triggers a `TG1WDT_SYS_RST` watchdog crash during SPI initialisation on the ESP32-C3. LovyanGFX initialises SPI2 directly via the ESP-IDF `spi_bus_initialize()` call, which works correctly. Don't switch back.
- **Credentials:** WiFi SSID/password and MQTT host are in `include/config.h`, which is gitignored. Never commit it — use `config.example.h` as the template.
- **No OTA yet:** Over-the-air updates will be added once the base firmware is stable.

---

*Kobo is watching. Kobo is waiting. Kobo will tell you when it's going to rain.*
