# 🦯 SmartGuide Cane v2 — On-Device AI

> **Obstacle detection cane with a quantized TFLite Micro model running directly on ESP32-CAM — no phone camera needed.**

---

## What Changed in v2

| Feature | v1 | v2 |
|---------|----|----|
| Object detection | Phone camera (optional) | **ESP32-CAM on-device TFLite** |
| `object` BLE field | Always `"unknown"` | `"person"`, `"stairs"`, `"car"`, etc. |
| AI model location | Flutter app | **Firmware (ESP32 flash)** |
| Hardware | ESP32 Dev Board | **ESP32-CAM (AI Thinker)** |
| PSRAM | Not required | **Required (4MB built-in)** |
| Confidence field | Not present | **0–255 in every payload** |
| Flutter camera | Required for AI | **Removed — not needed** |
| Inference rate | N/A | **~4Hz (every 5th sensor loop)** |

---

## System Architecture (v2)

```
┌─────────────────────────────────────────────────────────────────┐
│                     ESP32-CAM Firmware                           │
│                                                                  │
│  [HC-SR04 ×2]  [IR Sensor]          [OV2640 Camera]             │
│       ↓              ↓                     ↓                    │
│  readUltrasonic()  readIR()        captureGrayscaleFrame()       │
│       ↓              ↓                     ↓                    │
│       └──── EMA smooth ────┐     runInference() [TFLite Micro]  │
│                             ↓           ↓                       │
│                         fuseRisk() ←───┘  (distance + IR + AI)  │
│                             ↓                                    │
│               triggerHaptic() → vibration + buzzer               │
│                             ↓                                    │
│               BLE NOTIFY every 250ms                             │
│  {"distance_cm":42,"risk":"HIGH","object":"stairs",             │
│   "confidence":187,"ir":false,"ai_active":true,"ts":12345}      │
└─────────────────────────────────────────────────────────────────┘
                             ↓ Bluetooth LE
┌─────────────────────────────────────────────────────────────────┐
│                     Flutter App                                  │
│                                                                  │
│  BleService → parse JSON → CaneReading                          │
│  AlertService → TTS voice alert + phone vibration               │
│  SessionManager → log to SQLite                                  │
│  HomeScreen → RiskDisplay + AI Detection Card                    │
│  ResearchScreen → object distribution + avg confidence per class │
└─────────────────────────────────────────────────────────────────┘
```

---

## AI Model

**Architecture:** MobileNetV1 depth_multiplier=0.25, INT8 post-training quantization  
**Input:** 96×96×1 grayscale, INT8  
**Output:** 8 class scores, INT8  
**Flash size:** ~300 KB  
**Inference time:** ~120–180ms @ 240MHz with PSRAM tensor arena  
**Rate:** ~4Hz (runs every 5th sensor loop at 20Hz)

### Object Classes

| Index | Label | Force HIGH risk? |
|-------|-------|-----------------|
| 0 | unknown | No |
| 1 | person | No (uses distance) |
| 2 | chair | No |
| 3 | stairs | **Yes** |
| 4 | door | No |
| 5 | car | **Yes** |
| 6 | wall | No |
| 7 | pole | No |

---

## Risk Fusion Logic

```
fuseRisk(distance, IR, AI_result):
  if AI detected stairs OR car (above confidence threshold) → HIGH
  elif IR triggered OR distance < 50cm → HIGH
  elif distance < 100cm → MEDIUM
  elif distance < 150cm → LOW  
  elif AI detected person AND distance LOW → upgrade to MEDIUM
  else → NONE
```

---

## Training Your Own Model

```bash
cd ai-model/training/
pip install tensorflow pillow numpy tqdm

# Collect images into training_data/<class>/ folders
# (200+ images per class minimum, 500+ recommended)

python train_obstacle_model.py

# Copy output file:
cp ../firmware/esp32_cane_controller/obstacle_model_data.h \
   ../../firmware/esp32_cane_controller/
```

The training script:
- Trains MobileNetV1-0.25 on your images
- Applies INT8 post-training quantization with representative dataset
- Exports directly as `obstacle_model_data.h` C array
- Prints per-class accuracy and confusion matrix

---

## Updated BLE Payload

```json
{
  "distance_cm": 42,
  "risk": "HIGH",
  "object": "stairs",
  "confidence": 187,
  "ir": false,
  "ai_active": true,
  "ts": 123456
}
```

`confidence` maps to 0–255 unsigned score from INT8 model output. Divide by 255 for percentage.

---

## Arduino IDE Setup

```
Board:           AI Thinker ESP32-CAM
Partition:       Huge APP (3MB No OTA / 1MB SPIFFS)  ← required
PSRAM:           Enabled                             ← required
CPU Freq:        240MHz
```

Required libraries (Arduino Library Manager):
- `TensorFlowLite_ESP32` by TensorFlow
- `ESP32 BLE Arduino` (bundled with ESP32 board support)

---

## Firmware Files

| File | Purpose |
|------|---------|
| `esp32_cane_controller.ino` | Main firmware — sensor loop, AI, BLE, haptics |
| `config.h` | All pins, thresholds, TFLite settings |
| `camera.h` | OV2640 init, 96×96 grayscale capture |
| `obstacle_model.h` | TFLite Micro inference wrapper |
| `obstacle_model_data.h` | **Generated by training script** — C array |

---

## Resume Bullet (v2)

> **Built SmartGuide Cane v2, an assistive mobility prototype using ESP32-CAM + TensorFlow Lite Micro for on-device obstacle classification at 4Hz, fused with ultrasonic + IR sensors for real-time haptic and voice alerts over Bluetooth — no cloud or phone camera required.**
