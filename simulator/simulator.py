"""
DeskPet Simulator — Muni desktop display
=========================================
Renders Muni's animated 240x240 round face on a Windows PC using pygame.
Connects to the same MQTT broker as the real ESP32 and responds to all
deskpet/ topics, so you can test expression changes without hardware.

Requirements:
    pip install pygame paho-mqtt

Usage:
    python simulator.py
    python simulator.py --broker 192.168.2.14 --port 1883
"""

import argparse
import math
import sys
import threading
import time

import pygame
import paho.mqtt.client as mqtt

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
DISPLAY_SIZE   = 240
FPS            = 20
FRAME_MS       = 1000 // FPS

BROKER_HOST    = "192.168.2.14"
BROKER_PORT    = 1883
CLIENT_ID      = "deskpet-simulator"

TOPIC_EXPRESSION    = "deskpet/expression"
TOPIC_ANIMATION     = "deskpet/animation"
TOPIC_COMMAND       = "deskpet/command"
TOPIC_STATUS        = "deskpet/status"
TOPIC_CURRENT_EXPR  = "deskpet/current_expression"

# ---------------------------------------------------------------------------
# Colours  (RGB)
# ---------------------------------------------------------------------------
C_BLACK      = (  0,   0,   0)
C_WHITE      = (255, 255, 255)
C_PUPIL      = ( 20,  20,  20)
C_CHEEK      = (255, 182, 193)   # soft pink
C_CYAN       = (  0, 255, 255)   # accent — thinking dots, sparkles
C_BG         = (  0,   0,   0)

# ---------------------------------------------------------------------------
# Face geometry  (pixels, relative to display centre 120,120)
# ---------------------------------------------------------------------------
CX = CY = DISPLAY_SIZE // 2      # 120

EYE_SPACING   = 38
EYE_Y_OFFSET  = -18
EYE_RADIUS    = 22
PUPIL_RADIUS  = 11
SHINE_RADIUS  = 4

MOUTH_Y       = CY + 28
MOUTH_R       = 26
MOUTH_THICK   = 4

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
EXPRESSIONS = [
    "neutral", "happy", "sad", "surprised",
    "sleepy", "excited", "thinking",
]

state = {
    "expression":     "neutral",
    "breath_phase":   0.0,
    "blink_open":     1.0,
    "blink_timer":    time.time() + 3.0,
    "blinking":       False,
    "blink_frame":    0,
    "blink_frame_t":  0.0,
    "mqtt_status":    "disconnected",
    "anim":           None,        # one-shot animation name or None
    "anim_frame":     0,
    "anim_frame_t":   0.0,
}

state_lock = threading.Lock()


# ---------------------------------------------------------------------------
# Drawing helpers
# ---------------------------------------------------------------------------

def draw_arc(surface, cx, cy, r, thickness, start_deg, end_deg, colour):
    """Draw a thick arc by rendering a series of line segments."""
    # Normalise angles: 0° = top, clockwise (matching firmware convention)
    # pygame uses 0° = right, counter-clockwise — convert:
    #   pygame_angle = 90 - firmware_angle
    step = 1  # degrees per segment
    pts_outer = []
    pts_inner = []
    a = start_deg
    while a <= end_deg:
        rad = math.radians(a - 90)  # -90 converts from 12-o'clock to 3-o'clock origin
        cos_a, sin_a = math.cos(rad), math.sin(rad)
        pts_outer.append((cx + r * cos_a, cy + r * sin_a))
        pts_inner.append((cx + (r - thickness) * cos_a, cy + (r - thickness) * sin_a))
        a += step

    if len(pts_outer) < 2:
        return

    # Draw as filled polygon: outer ring → inner ring reversed
    poly = pts_outer + list(reversed(pts_inner))
    pygame.draw.polygon(surface, colour, [(int(x), int(y)) for x, y in poly])


def draw_sparkle(surface, cx, cy, size, colour):
    """4-point star sparkle."""
    pygame.draw.line(surface, colour, (cx, cy - size), (cx, cy + size), 1)
    pygame.draw.line(surface, colour, (cx - size, cy), (cx + size, cy), 1)
    d = (size * 6) // 10
    pygame.draw.line(surface, colour, (cx - d, cy - d), (cx + d, cy + d), 1)
    pygame.draw.line(surface, colour, (cx - d, cy + d), (cx + d, cy - d), 1)


def draw_eye(surface, ex, ey, open_amount, expr):
    """Draw one eye."""
    if open_amount <= 0.05:
        pygame.draw.line(surface, C_WHITE,
                         (ex - EYE_RADIUS, ey), (ex + EYE_RADIUS, ey), 2)
        return

    eye_h = max(2, int(EYE_RADIUS * open_amount))

    if expr == "happy":
        draw_arc(surface, ex, ey + 6, EYE_RADIUS, MOUTH_THICK + 2, 210, 330, C_WHITE)

    elif expr == "excited":
        pygame.draw.circle(surface, C_WHITE, (ex, ey), EYE_RADIUS + 3)
        pygame.draw.circle(surface, C_PUPIL, (ex, ey), PUPIL_RADIUS)
        pygame.draw.circle(surface, C_WHITE, (ex + 5, ey - 6), SHINE_RADIUS)
        pygame.draw.circle(surface, C_WHITE, (ex - 5, ey - 2), SHINE_RADIUS - 2)

    elif expr in ("sad", "thinking", "neutral"):
        pygame.draw.ellipse(surface, C_WHITE,
                            (ex - EYE_RADIUS, ey - eye_h, EYE_RADIUS * 2, eye_h * 2))
        pygame.draw.circle(surface, C_PUPIL, (ex, ey), PUPIL_RADIUS)
        pygame.draw.circle(surface, C_WHITE, (ex + 5, ey - 5), SHINE_RADIUS)

    elif expr == "sleepy":
        pygame.draw.ellipse(surface, C_WHITE,
                            (ex - EYE_RADIUS, ey - eye_h, EYE_RADIUS * 2, eye_h * 2))
        # Eyelid: cover top half
        pygame.draw.rect(surface, C_BG,
                         (ex - EYE_RADIUS - 2, ey - eye_h - 2,
                          (EYE_RADIUS + 2) * 2, eye_h + 2))
        pygame.draw.circle(surface, C_PUPIL, (ex, ey + 4), PUPIL_RADIUS - 2)

    elif expr == "surprised":
        pygame.draw.circle(surface, C_WHITE, (ex, ey), EYE_RADIUS + 4)
        pygame.draw.circle(surface, C_PUPIL, (ex, ey), PUPIL_RADIUS + 2)
        pygame.draw.circle(surface, C_WHITE, (ex + 5, ey - 5), SHINE_RADIUS)

    else:
        pygame.draw.ellipse(surface, C_WHITE,
                            (ex - EYE_RADIUS, ey - eye_h, EYE_RADIUS * 2, eye_h * 2))
        pygame.draw.circle(surface, C_PUPIL, (ex, ey), PUPIL_RADIUS)
        pygame.draw.circle(surface, C_WHITE, (ex + 5, ey - 5), SHINE_RADIUS)


def draw_mouth(surface, expr, open_amount=1.0):
    mx, my = CX, MOUTH_Y

    if expr == "happy":
        draw_arc(surface, mx, my - MOUTH_R + 8, MOUTH_R, MOUTH_THICK, 140, 400, C_WHITE)

    elif expr == "excited":
        draw_arc(surface, mx, my - MOUTH_R + 4, MOUTH_R + 6, MOUTH_THICK + 3, 130, 410, C_WHITE)

    elif expr == "sad":
        draw_arc(surface, mx, my + MOUTH_R - 8, MOUTH_R, MOUTH_THICK, 320, 220, C_WHITE)

    elif expr == "surprised":
        r = max(4, int(14 * open_amount))
        pygame.draw.ellipse(surface, C_WHITE,
                            (mx - r - 4, my - r, (r + 4) * 2, r * 2))
        pygame.draw.ellipse(surface, C_BG,
                            (mx - r, my - (r - 4), r * 2, max(1, (r - 4) * 2)))

    elif expr == "sleepy":
        pygame.draw.ellipse(surface, C_WHITE, (mx - 12, my - 6, 24, 12))
        pygame.draw.ellipse(surface, C_BG,    (mx -  8, my - 3, 16,  6))

    elif expr == "thinking":
        pygame.draw.line(surface, C_WHITE, (mx - 4, my + 2), (mx + 14, my - 4), 2)

    else:  # neutral
        pygame.draw.line(surface, C_WHITE, (mx - 18, my),     (mx + 18, my),     2)
        pygame.draw.line(surface, C_WHITE, (mx - 18, my + 1), (mx + 18, my + 1), 2)


def draw_face(surface, expr, breath_scale, blink_open_l, blink_open_r):
    """Render one complete frame onto surface."""
    surface.fill(C_BG)

    breath_offset = int(breath_scale * 4)

    lx = CX - EYE_SPACING
    ly = CY + EYE_Y_OFFSET + breath_offset
    rx = CX + EYE_SPACING
    ry = CY + EYE_Y_OFFSET + breath_offset

    right_open = blink_open_r
    if expr == "thinking":
        right_open *= 0.55

    draw_eye(surface, lx, ly, blink_open_l, expr)
    draw_eye(surface, rx, ry, right_open,   expr)

    if expr == "happy":
        pygame.draw.ellipse(surface, C_CHEEK, (lx - 10 - 16, ly + 26 - 8, 32, 16))
        pygame.draw.ellipse(surface, C_CHEEK, (rx + 10 - 16, ry + 26 - 8, 32, 16))

    elif expr == "excited":
        pygame.draw.ellipse(surface, C_CHEEK, (lx - 14 - 22, ly + 30 - 11, 44, 22))
        pygame.draw.ellipse(surface, C_CHEEK, (rx + 14 - 22, ry + 30 - 11, 44, 22))
        draw_sparkle(surface, lx - 30, ly - 24, 6, C_CYAN)
        draw_sparkle(surface, rx + 30, ry - 24, 6, C_CYAN)
        draw_sparkle(surface, lx + 24, ly - 30, 4, C_CYAN)
        draw_sparkle(surface, rx - 24, ry - 30, 4, C_CYAN)

    draw_mouth(surface, expr)

    if expr == "thinking":
        for i in range(3):
            pygame.draw.circle(surface, C_CYAN, (CX + 50 + i * 12, CY - 50), 4)


# ---------------------------------------------------------------------------
# Clip to round display (apply circular mask each frame)
# ---------------------------------------------------------------------------
def apply_round_mask(surface, mask):
    """Blit a pre-built alpha mask onto the surface to clip it to a circle."""
    surface.blit(mask, (0, 0))


def build_round_mask(size):
    """Build a surface with a circular hole — everything outside is black."""
    mask = pygame.Surface((size, size), pygame.SRCALPHA)
    mask.fill((0, 0, 0, 255))
    pygame.draw.circle(mask, (0, 0, 0, 0), (size // 2, size // 2), size // 2)
    return mask


# ---------------------------------------------------------------------------
# One-shot animation helpers
# ---------------------------------------------------------------------------

def tick_bounce(surface, frame):
    """Return True when animation is done."""
    offsets = [0, -8, -14, -8, 0, 8, 4, 0]
    yoff = offsets[frame % len(offsets)]
    surface.fill(C_BG)
    draw_eye(surface, CX - EYE_SPACING, CY + EYE_Y_OFFSET + yoff, 1.0, "happy")
    draw_eye(surface, CX + EYE_SPACING, CY + EYE_Y_OFFSET + yoff, 1.0, "happy")
    pygame.draw.ellipse(surface, C_CHEEK,
                        (CX - EYE_SPACING - 10 - 16, CY + EYE_Y_OFFSET + yoff + 26 - 8, 32, 16))
    pygame.draw.ellipse(surface, C_CHEEK,
                        (CX + EYE_SPACING + 10 - 16, CY + EYE_Y_OFFSET + yoff + 26 - 8, 32, 16))
    draw_mouth(surface, "happy")
    return frame >= len(offsets) - 1


def tick_yawn(surface, frame, total=20):
    """Return True when animation is done."""
    t = frame / total
    eye_open  = max(0.0, (1.0 - t * 2.5) if t < 0.4 else
                         ((t - 0.7) * 3.3  if t > 0.7 else 0.0))
    mouth_open = max(0.0, t * 2.0 if t < 0.5 else 1.0 - (t - 0.5) * 2.0)
    surface.fill(C_BG)
    draw_eye(surface, CX - EYE_SPACING, CY + EYE_Y_OFFSET, eye_open, "sleepy")
    draw_eye(surface, CX + EYE_SPACING, CY + EYE_Y_OFFSET, eye_open, "sleepy")
    r_w = int(18 * mouth_open) + 4
    r_h = int(20 * mouth_open) + 2
    if r_w > 0 and r_h > 0:
        pygame.draw.ellipse(surface, C_WHITE,
                            (CX - r_w, MOUTH_Y - r_h, r_w * 2, r_h * 2))
        if mouth_open > 0.3:
            ir = int(14 * mouth_open)
            if ir > 0:
                pygame.draw.ellipse(surface, C_BG,
                                    (CX - ir, MOUTH_Y - ir, ir * 2, ir * 2))
    return frame >= total - 1


# ---------------------------------------------------------------------------
# Idle animation tick (breathing + blinking)
# ---------------------------------------------------------------------------

def tick_idle(st):
    now = time.time()

    # Breathing — 4-second sine cycle
    st["breath_phase"] += 0.0785
    if st["breath_phase"] > 2 * math.pi:
        st["breath_phase"] -= 2 * math.pi
    breath_scale = (math.sin(st["breath_phase"]) + 1.0) * 0.5

    # Blinking
    blink_open = 1.0
    if not st["blinking"]:
        if now >= st["blink_timer"]:
            st["blinking"]      = True
            st["blink_frame"]   = 0
            st["blink_frame_t"] = now
    else:
        if now - st["blink_frame_t"] > 0.06:
            st["blink_frame"]   += 1
            st["blink_frame_t"]  = now

        frame_map = {0: 1.0, 1: 0.5, 2: 0.05, 3: 0.5}
        blink_open = frame_map.get(st["blink_frame"], 1.0)

        if st["blink_frame"] >= 4:
            st["blinking"]    = False
            st["blink_frame"] = 0
            st["blink_timer"] = now + 3.0 + (now % 4.0)
            blink_open        = 1.0

    return breath_scale, blink_open


# ---------------------------------------------------------------------------
# MQTT
# ---------------------------------------------------------------------------

def on_connect(client, userdata, flags, rc):
    with state_lock:
        state["mqtt_status"] = "connected" if rc == 0 else f"error:{rc}"
    if rc == 0:
        client.subscribe(TOPIC_EXPRESSION)
        client.subscribe(TOPIC_ANIMATION)
        client.subscribe(TOPIC_COMMAND)
        client.publish(TOPIC_STATUS, "online", retain=True)
        print(f"[MQTT] Connected to broker")
    else:
        print(f"[MQTT] Connect failed: rc={rc}")


def on_disconnect(client, userdata, rc):
    with state_lock:
        state["mqtt_status"] = "disconnected"
    print("[MQTT] Disconnected")


def on_message(client, userdata, msg):
    topic   = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore").strip()
    print(f"[MQTT] {topic}: {payload}")

    with state_lock:
        if topic == TOPIC_EXPRESSION:
            if payload in EXPRESSIONS:
                state["expression"] = payload
                state["anim"]       = None
        elif topic == TOPIC_ANIMATION:
            if payload in ("bounce", "blink", "yawn", "breathe"):
                state["anim"]        = payload
                state["anim_frame"]  = 0
                state["anim_frame_t"] = time.time()
        elif topic == TOPIC_COMMAND:
            if payload == "restart":
                print("[Simulator] Received restart — resetting to neutral")
                state["expression"] = "neutral"
                state["anim"]       = None
            elif payload == "sleep":
                state["expression"] = "sleepy"
            elif payload == "wake":
                state["expression"] = "neutral"


def start_mqtt(broker_host, broker_port):
    client = mqtt.Client(client_id=CLIENT_ID, clean_session=True)
    client.on_connect    = on_connect
    client.on_disconnect = on_disconnect
    client.on_message    = on_message
    client.will_set(TOPIC_STATUS, "offline", retain=True)

    try:
        client.connect(broker_host, broker_port, keepalive=60)
    except Exception as e:
        print(f"[MQTT] Could not connect to {broker_host}:{broker_port}: {e}")
        print("[MQTT] Running in offline mode — expressions respond to keyboard only")

    client.loop_start()
    return client


# ---------------------------------------------------------------------------
# Keyboard shortcuts (for quick testing without MQTT)
# ---------------------------------------------------------------------------
KEY_EXPRESSIONS = {
    pygame.K_1: "neutral",
    pygame.K_2: "happy",
    pygame.K_3: "sad",
    pygame.K_4: "surprised",
    pygame.K_5: "sleepy",
    pygame.K_6: "excited",
    pygame.K_7: "thinking",
}

KEY_ANIMATIONS = {
    pygame.K_b: "bounce",
    pygame.K_y: "yawn",
}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="DeskPet Muni simulator")
    parser.add_argument("--broker", default=BROKER_HOST, help="MQTT broker hostname/IP")
    parser.add_argument("--port",   default=BROKER_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()

    pygame.init()
    pygame.display.set_caption("DeskPet — Muni simulator  |  1-7: expressions  b: bounce  y: yawn")
    screen = pygame.display.set_mode((DISPLAY_SIZE, DISPLAY_SIZE))
    clock  = pygame.time.Clock()

    face_surface = pygame.Surface((DISPLAY_SIZE, DISPLAY_SIZE))
    round_mask   = build_round_mask(DISPLAY_SIZE)

    mqtt_client = start_mqtt(args.broker, args.port)

    last_anim_frame_t = time.time()

    print("DeskPet Muni simulator running.")
    print("Keys: 1=neutral 2=happy 3=sad 4=surprised 5=sleepy 6=excited 7=thinking")
    print("      b=bounce  y=yawn  q=quit")

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_q:
                    running = False
                elif event.key in KEY_EXPRESSIONS:
                    with state_lock:
                        state["expression"] = KEY_EXPRESSIONS[event.key]
                        state["anim"]       = None
                elif event.key in KEY_ANIMATIONS:
                    with state_lock:
                        state["anim"]        = KEY_ANIMATIONS[event.key]
                        state["anim_frame"]  = 0
                        state["anim_frame_t"] = time.time()

        with state_lock:
            expr      = state["expression"]
            anim      = state["anim"]
            anim_frame = state["anim_frame"]
            mqtt_status = state["mqtt_status"]

        # --- One-shot animations ---
        now = time.time()
        anim_done = False
        if anim is not None:
            # Advance anim frame every 50 ms (matching firmware 50ms tick)
            if now - last_anim_frame_t >= 0.05:
                last_anim_frame_t = now
                if anim == "bounce":
                    anim_done = tick_bounce(face_surface, anim_frame)
                elif anim == "yawn":
                    anim_done = tick_yawn(face_surface, anim_frame)
                else:
                    anim_done = True  # unknown — clear immediately

                with state_lock:
                    if anim_done:
                        if anim == "yawn":
                            state["expression"] = "sleepy"
                        state["anim"]       = None
                        state["anim_frame"] = 0
                    else:
                        state["anim_frame"] += 1
        else:
            # --- Idle animation ---
            breath_scale, blink_open = tick_idle(state)
            draw_face(face_surface, expr, breath_scale, blink_open, blink_open)

        # Apply round mask to clip to circle
        apply_round_mask(face_surface, round_mask)

        screen.blit(face_surface, (0, 0))
        pygame.display.set_caption(
            f"DeskPet — Muni | {expr} | MQTT: {mqtt_status}"
        )
        pygame.display.flip()
        clock.tick(FPS)

    mqtt_client.publish(TOPIC_STATUS, "offline", retain=True)
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    pygame.quit()
    sys.exit(0)


if __name__ == "__main__":
    main()
