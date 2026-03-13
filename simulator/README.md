# DeskPet Simulator

A Python desktop app that simulates Muni's 240×240 round GC9A01 display on Windows (or any platform with Python).

Renders the same programmatic face expressions as the firmware and connects to the same MQTT broker, so you can test expression changes and automations without physical hardware.

## Setup

```bash
cd simulator

# Create a virtual environment (recommended)
python -m venv venv
venv\Scripts\activate      # Windows
# source venv/bin/activate # Linux/Mac

pip install -r requirements.txt
```

## Running

```bash
# Default — connects to broker at 192.168.2.14:1883
python simulator.py

# Custom broker
python simulator.py --broker 192.168.2.14 --port 1883
```

The simulator window title shows the current expression and MQTT connection status.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| `1` | neutral |
| `2` | happy |
| `3` | sad |
| `4` | surprised |
| `5` | sleepy |
| `6` | excited |
| `7` | thinking |
| `b` | bounce animation |
| `y` | yawn animation |
| `q` | quit |

## MQTT topics

The simulator subscribes to all the same topics as the ESP32:

| Topic | Effect |
|-------|--------|
| `deskpet/expression` | Changes the displayed expression |
| `deskpet/animation` | Triggers `bounce` or `yawn` |
| `deskpet/command` | `restart` / `sleep` / `wake` |

It also publishes `online` / `offline` to `deskpet/status` (with LWT), matching the real device.

## Testing without a broker

The simulator runs fine if the broker is unreachable — it just prints a warning and runs in offline mode. Use the keyboard shortcuts to test expressions.
