# Homey MQTT Flows — DeskPet Integration

This document describes how to configure Homey to publish MQTT messages that
trigger expressions and animations on the DeskPet.

## Prerequisites

- Homey Self-Hosted running on your home server
- Mosquitto MQTT broker running at `192.168.2.14:1883`
- The "MQTT Client" app installed in Homey (or equivalent)
- DeskPet ESP32 firmware flashed and connected to the same network

---

## How It Works

Homey flows use the MQTT Client app's "Publish a message" action card.
When a trigger fires (person arrives, sensor opens, schedule, etc.), Homey
publishes a payload to the topic `deskpet/expression` or `deskpet/animation`.

The ESP32 is subscribed to these topics and reacts immediately.

---

## Flow Recipes

### 1. Someone Arrives Home → Happy

**Trigger:** Presence zone "Home" — a person enters
**Condition:** (optional) Between 07:00 and 23:00
**Action:** MQTT Publish
```
Topic:   deskpet/expression
Payload: happy
```

---

### 2. Everyone in Bed → Sleepy

**Trigger:** Virtual device "Alle i seng" is switched ON
  (or: last person's presence enters bedroom zone after 21:00)
**Action:** MQTT Publish
```
Topic:   deskpet/expression
Payload: sleepy
```

---

### 3. Front Door Opens → Surprised

**Trigger:** Door sensor "Hoveddør" contact opened
**Action:** MQTT Publish
```
Topic:   deskpet/expression
Payload: surprised
```

---

### 4. Good Morning → Yawn Animation

**Trigger:** Time is 07:00 (or alarm trigger from phone)
**Action:** MQTT Publish
```
Topic:   deskpet/animation
Payload: yawn
```

---

### 5. Rain Forecast Today → Sad

**Trigger:** Weather app — precipitation forecast > 0.5mm for today
  (or: any weather trigger that indicates rain)
**Action:** MQTT Publish
```
Topic:   deskpet/expression
Payload: sad
```

---

### 6. Motion Detected at Night → Surprised

**Trigger:** Motion sensor fires between 23:00 and 06:00
**Action:** MQTT Publish
```
Topic:   deskpet/expression
Payload: surprised
```

---

## Testing Flows from the Command Line

You can test without Homey using the `mosquitto_pub` command on your home server:

```bash
# Trigger happy expression
mosquitto_pub -h 192.168.2.14 -t "deskpet/expression" -m "happy"

# Trigger yawn animation
mosquitto_pub -h 192.168.2.14 -t "deskpet/animation" -m "yawn"

# Check that the ESP32 is online
mosquitto_sub -h 192.168.2.14 -t "deskpet/status"

# Watch all deskpet traffic
mosquitto_sub -h 192.168.2.14 -t "deskpet/#" -v
```

---

## MQTT Topic Reference

See the main [README.md](../README.md#mqtt-topic-structure) for the full topic table.
