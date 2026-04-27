/*
 * SmartGuide Cane v2 — config.h
 * ─────────────────────────────────────────────────────────────────────────────
 * Hardware upgrade: ESP32-CAM (AI Thinker module)
 *   - Replaces plain ESP32 + external camera
 *   - Built-in OV2640 camera
 *   - 4MB PSRAM for frame buffers + TFLite model arena
 *
 * PIN NOTE: ESP32-CAM has different pinout than ESP32 Dev Board.
 *   GPIO 1/3 = UART TX/RX (used for Serial — disconnect during upload)
 *   GPIO 4   = onboard LED flash (avoid using for other purposes)
 *   Camera pins are fixed by the AI Thinker board layout (see below)
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

// ── Ultrasonic HC-SR04 ────────────────────────────────────────────────────────
// ESP32-CAM has limited free GPIOs — these are the safe ones
#define PIN_TRIG        12   // GPIO 12 (free on AI Thinker when SD not used)
#define PIN_ECHO        13   // GPIO 13

// ── IR obstacle sensor (active LOW) ──────────────────────────────────────────
#define PIN_IR          15   // GPIO 15

// ── Actuators ─────────────────────────────────────────────────────────────────
#define PIN_VIBRATION   14   // GPIO 14 → NPN transistor base
#define PIN_BUZZER      2    // GPIO 2  (shares with onboard LED — ok for buzzer)

// ── Status LED ────────────────────────────────────────────────────────────────
#define PIN_LED         33   // GPIO 33 = small red LED on AI Thinker board

// ── Distance thresholds (centimeters) ────────────────────────────────────────
#define THRESH_HIGH     50
#define THRESH_MEDIUM   100
#define THRESH_LOW      150
#define DIST_MAX        400

// ── Sensor tuning ─────────────────────────────────────────────────────────────
#define SMOOTH_ALPHA        0.30f
#define LOOP_DELAY_MS       50       // sensor loop ~20Hz
#define SEND_INTERVAL_MS    250      // BLE notify interval (slightly relaxed for AI overhead)
#define VIB_COOLDOWN_MS     800

// ── Buzzer frequencies ────────────────────────────────────────────────────────
#define BUZZ_HIGH_FREQ  2000
#define BUZZ_MED_FREQ   1200
#define BUZZ_HIGH_DUR   400
#define BUZZ_MED_DUR    200

// ── BLE ───────────────────────────────────────────────────────────────────────
#define BLE_DEVICE_NAME "SmartGuide-Cane"
#define SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ── TFLite Micro ──────────────────────────────────────────────────────────────
// Model input: 96×96 grayscale (INT8 quantized MobileNetV1 0.25)
#define CAM_WIDTH       96
#define CAM_HEIGHT      96
// Tensor arena size — 100KB in PSRAM (ESP32-CAM has 4MB PSRAM)
#define TENSOR_ARENA_KB 100
#define TENSOR_ARENA_SIZE (TENSOR_ARENA_KB * 1024)

// AI inference throttle: run model every N sensor loops (not every loop)
// At 20Hz loop + 5 skip = inference ~4Hz — enough for walking speed
#define AI_INFERENCE_EVERY_N_LOOPS  5

// Minimum confidence to report an object (0–255 for INT8 model output)
// Scores below this threshold → label stays "unknown"
#define AI_CONFIDENCE_THRESHOLD  140   // ~55% confidence

// ── Object class labels (must match model output order) ──────────────────────
// These map to the output tensor indices of our trained model.
// Order MUST match the label order used during model training.
#define OBJ_UNKNOWN   0
#define OBJ_PERSON    1
#define OBJ_CHAIR     2
#define OBJ_STAIRS    3
#define OBJ_DOOR      4
#define OBJ_CAR       5
#define OBJ_WALL      6
#define OBJ_POLE      7
#define NUM_CLASSES   8

// Human-readable names (index matches above defines)
static const char* OBJ_LABELS[NUM_CLASSES] = {
  "unknown",
  "person",
  "chair",
  "stairs",
  "door",
  "car",
  "wall",
  "pole"
};

// Per-object risk override: does detecting this object force HIGH risk?
// true = always HIGH regardless of distance sensor reading
static const bool OBJ_FORCE_HIGH[NUM_CLASSES] = {
  false,  // unknown
  false,  // person   (use distance to grade)
  false,  // chair
  true,   // stairs   → ALWAYS HIGH
  false,  // door
  true,   // car      → ALWAYS HIGH
  false,  // wall
  false,  // pole
};
