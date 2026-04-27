# SmartGuide Cane v2 — Wiring Diagram (ESP32-CAM)

## Hardware Upgrade: ESP32-CAM (AI Thinker)

The plain ESP32 is replaced by the **ESP32-CAM (AI Thinker)** module which adds:
- Built-in OV2640 camera (for TFLite inference)
- 4MB PSRAM (for tensor arena + frame buffer)
- Same BLE/WiFi as ESP32

**Cost: ~$10–15 vs ~$8 for plain ESP32 — well worth it for on-device AI.**

---

## Free GPIO Pins on AI Thinker (after camera uses its fixed pins)

| GPIO | Safe to use? | Notes                                      |
|------|--------------|--------------------------------------------|
| 12   | ✅ Yes        | Used for TRIG (ultrasonic)                 |
| 13   | ✅ Yes        | Used for ECHO (ultrasonic)                 |
| 14   | ✅ Yes        | Used for vibration motor                   |
| 15   | ✅ Yes        | Used for IR sensor                         |
| 2    | ⚠️ Shared    | Onboard LED flash — also used for buzzer   |
| 33   | ✅ Yes        | Small red LED (status indicator)           |
| 1/3  | ⚠️ UART      | Serial TX/RX — disconnect during upload    |
| 4    | ❌ Avoid      | Flash LED + SD card — leave free           |
| 0    | ❌ Boot pin   | Must be HIGH during normal boot            |

---

## Wiring Diagram

```
ESP32-CAM (AI Thinker)
┌─────────────────────────────────────────┐
│  OV2640 camera (built-in, fixed pins)   │
│                                         │
│  3.3V ──── HC-SR04 VCC  [if 3V3 type]  │
│   5V  ──── HC-SR04 VCC  [if 5V type]   │
│   GND ──── HC-SR04 GND                  │
│  GPIO12 ── HC-SR04 TRIG                 │
│  GPIO13 ── HC-SR04 ECHO ─┐             │
│                           ├─ 1kΩ → GND  │  ← voltage divider if 5V sensor
│                                         │
│  3.3V ──── IR Sensor VCC                │
│   GND ──── IR Sensor GND                │
│  GPIO15 ── IR Sensor OUT                │
│                                         │
│  GPIO14 ─── 1kΩ ── NPN Base (2N2222)   │
│  NPN Collector ──── Vib Motor (+)       │
│  NPN Emitter ──── GND                   │
│  Vib Motor (-) ──── GND                 │
│  [1N4007 flyback diode across motor]    │
│                                         │
│  GPIO2 ──── Passive Buzzer (+)          │
│   GND ───── Passive Buzzer (-)          │
│                                         │
│  GPIO33 ─── 220Ω ─── LED (+) ─── GND   │
│                                         │
│  5V input ─ TP4056 output               │
│  [3.7V LiPo → TP4056 → 5V boost → VCC] │
└─────────────────────────────────────────┘
```

---

## Power Notes

ESP32-CAM draws significantly more current than plain ESP32:
- Idle + BLE:           ~100mA
- Camera capture:       +50mA burst (~150ms per frame)
- TFLite inference:     +60mA (~120–180ms)
- Vibration motor:      +80mA peak
- **Peak total:         ~290mA**
- **Average:            ~150mA** (at 4Hz inference, 20Hz sensor loop)

**Recommended:** 2500mAh LiPo → ~14 hours runtime  
**Minimum:** 1000mAh → ~5 hours

Power chain: LiPo → TP4056 charger → MT3608 boost (5V) → ESP32-CAM 5V pin  
OR: Regulated 5V USB power bank with passthrough charging

---

## Arduino IDE Settings for ESP32-CAM

```
Board:              AI Thinker ESP32-CAM
Partition Scheme:   Huge APP (3MB No OTA / 1MB SPIFFS)  ← CRITICAL for TFLite
PSRAM:              Enabled                             ← CRITICAL
CPU Frequency:      240MHz
Flash Frequency:    80MHz
Flash Mode:         QIO
Upload Speed:       115200
```

**Upload method:** Use FTDI adapter (ESP32-CAM has no USB)
- Connect FTDI GND → GND
- Connect FTDI TX → GPIO3 (RX)
- Connect FTDI RX → GPIO1 (TX)
- Connect FTDI 5V → 5V
- **Pull GPIO0 to GND during upload, then release for normal run**

---

## Bill of Materials v2

| Component                    | Approx Cost |
|------------------------------|-------------|
| ESP32-CAM (AI Thinker)       | $10–15      |
| FTDI USB-Serial adapter      | $5–8        |
| HC-SR04 (×2)                 | $2–4        |
| IR Sensor Module             | $1–2        |
| Vibration Motor              | $1–2        |
| Passive Buzzer               | $0.50       |
| NPN Transistors (2N2222)     | $0.50       |
| 1N4007 flyback diode         | $0.25       |
| 2500mAh LiPo + TP4056        | $6–10       |
| MT3608 boost converter (5V)  | $1–2        |
| Misc (wires, resistors, PCB) | $5          |
| **Total**                    | **~$35–50** |

(~$10–15 more than v1 — worth it for on-device AI)

---

## Key Differences from v1

| Feature           | v1 (ESP32)          | v2 (ESP32-CAM)              |
|-------------------|---------------------|-----------------------------|
| Object detection  | Phone camera        | **On-device TFLite Micro**  |
| `object` field    | `"unknown"`         | `"person"`, `"stairs"`, etc.|
| Camera            | External / phone    | **Built-in OV2640**         |
| PSRAM             | None / external     | **4MB built-in**            |
| AI inference rate | N/A                 | **~4Hz**                    |
| BLE payload       | 5 fields            | **7 fields + confidence**   |
| Flash required    | 1MB sketch          | **3MB (Huge APP partition)** |
