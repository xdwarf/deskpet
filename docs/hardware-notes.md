# Hardware Notes — DeskPet

Detailed notes on hardware-specific quirks, setup steps, and things to watch out for.

---

## ESP32-C3 Mini

### USB Serial / Flashing

- The ESP32-C3 Mini uses a USB-C connector and exposes a CDC serial port directly (no separate FTDI chip).
- On first use, you may need to hold the **BOOT** button while pressing **RESET** to enter flash mode. After the first successful flash, PlatformIO handles this automatically.
- If the port doesn't appear on Windows, install the [WCH CH343 driver](https://www.wch-ic.com/products/CH343.html) (some C3 Mini boards use a WCH chip for USB).

### GPIO Warnings

- **GPIO2** is a strapping pin — avoid using it for SPI/output until after boot.
- **GPIO8** is also a strapping pin (controls ROM message print level) but is safe for use as display RST once the device is booted.
- **GPIO19 / GPIO20** are used for native USB on the ESP32-C3 — leave these free.

### Recommended PlatformIO board ID

```ini
board = esp32-c3-devkitm-1
```

If your board behaves oddly, try `lolin_c3_mini` — there are slight UART differences.

---

## GC9A01 Round Display

### Power

- **Must be powered by 3.3V.** Some GC9A01 breakout boards have a 3.3V regulator onboard and accept 5V on VCC — check your specific board. Feeding 5V directly to the GC9A01 chip will destroy it.

### Backlight

- The BL (backlight) pin can be:
  - Tied directly to 3.3V (always on, simplest)
  - Connected to a GPIO with PWM capability for brightness control
- We use GPIO3 as the backlight pin. `ledcWrite()` can dim it if needed later.

### SPI Speed

- Start at `SPI_FREQUENCY = 40000000` (40 MHz). The GC9A01 datasheet specs up to 80 MHz.
- If you see corrupted pixels, reduce to 27 MHz and check your wire lengths (keep SPI wires short — under 10 cm ideally).

### TFT_eSPI colour order

- If colours appear wrong (e.g., red and blue swapped), try adding `#define TFT_RGB_ORDER TFT_RGB` to `User_Setup.h`.

---

## Raspberry Pi 3B+

### Performance expectations for Stage 2

| Task | Estimated time on Pi 3B+ |
|---|---|
| Wake word detection (openwakeword) | Real-time, ~10% CPU |
| Record 8s of audio | 8s (real-time) |
| Whisper "base" transcription | ~5–8 seconds |
| Claude API call (haiku) | ~1–3 seconds (network dependent) |
| gTTS generation | ~2–3 seconds |
| AirCast cast request | <1 second |
| **Total latency after wake word** | **~12–18 seconds** |

This is acceptable for a desk companion. If it feels too slow, consider:
- Hosting Whisper on the home server instead (much faster)
- Using the Whisper API (cloud, ~1s, costs money)

### USB Microphone

The USB webcam mic should appear as an ALSA device. To find its device index:

```bash
python3 -c "
import pyaudio
p = pyaudio.PyAudio()
for i in range(p.get_device_count()):
    d = p.get_device_info_by_index(i)
    if d['maxInputChannels'] > 0:
        print(i, d['name'])
"
```

Update `audio.mic_device_index` in `config.yaml` with the correct index.

### Auto-start on boot

To run the voice assistant automatically when the Pi boots:

```bash
# Create a systemd service
sudo nano /etc/systemd/system/deskpet.service
```

```ini
[Unit]
Description=DeskPet Voice Assistant
After=network.target

[Service]
User=pi
WorkingDirectory=/home/pi/deskpet/voice-assistant
ExecStart=/home/pi/deskpet/voice-assistant/venv/bin/python src/main.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable deskpet
sudo systemctl start deskpet
sudo journalctl -u deskpet -f  # follow logs
```

---

## AirCast

AirCast (Docker container on the home server) casts audio to AirPlay receivers like Google Nest speakers.

### Finding your AirCast API

The API endpoint depends on which AirCast image you're running. SSH to your server and check:

```bash
docker ps | grep aircast
docker logs <container_name>
```

Common endpoints to try:
- `http://192.168.2.14:5000/cast`
- `http://192.168.2.14:8080/play`

### Testing AirCast manually

```bash
# From the Pi or home server
curl -X POST http://192.168.2.14:5000/cast \
  -F "audio=@/tmp/test.mp3" \
  -F "speaker=Stuen"
```

---

## Wake Word — "Hey Neo"

The default config uses `hey_jarvis` from openwakeword as a placeholder.
Training a custom "Hey Neo" model requires:

1. Recording ~500 positive examples of "Hey Neo" in various voices/conditions
2. Using the [openwakeword training toolkit](https://github.com/dscripka/openWakeWord/tree/main/openwakeword/custom_verifier_models)
3. Exporting as `.onnx` and placing in the voice-assistant directory
4. Updating `wake_word.model` in `config.yaml`

Alternatively: [Picovoice Porcupine](https://picovoice.ai/platform/porcupine/) lets you train custom wake words online with a free tier.
