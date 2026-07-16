/*
 * obstacle_model.h — TFLite Micro stub model
 *
 * Right now this is a STUB — it doesn't detect anything.
 * Replace obstacle_model_data.h with your trained model later.
 *
 * The stub lets everything else compile and run so you can test
 * the camera, BLE, and sensor code before the AI model is ready.
 */

#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// STUB MODE
// obstacle_model_data.h doesn't exist yet — that's fine.
// The stub always returns "unknown" so sensors still work.
// When you train your model, drop obstacle_model_data.h into the
// sketch folder and delete the #define USING_STUB_MODEL line.
// ─────────────────────────────────────────────────────────────────────────────
#define USING_STUB_MODEL

// Object class labels — index matches model output tensor
// IMPORTANT: this order must match exactly when you train the real model
#define OBJ_UNKNOWN  0
#define OBJ_PERSON   1
#define OBJ_CHAIR    2
#define OBJ_STAIRS   3
#define OBJ_DOOR     4
#define OBJ_CAR      5
#define OBJ_WALL     6
#define OBJ_POLE     7
#define NUM_CLASSES  8

static const char* OBJ_LABELS[NUM_CLASSES] = {
  "unknown", "person", "chair", "stairs",
  "door",    "car",    "wall",  "pole"
};

// Objects that always force HIGH risk regardless of distance
// Stairs and cars are critical — even far away they're dangerous
static const bool OBJ_FORCE_HIGH[NUM_CLASSES] = {
  false,  // unknown
  false,  // person  (use distance to decide)
  false,  // chair
  true,   // stairs  → ALWAYS HIGH
  false,  // door
  true,   // car     → ALWAYS HIGH
  false,  // wall
  false,  // pole
};

// Result returned by runInference()
struct InferenceResult {
  int         classIndex;  // 0–7
  int         confidence;  // 0–255
  const char* label;       // points to OBJ_LABELS
  bool        forceHigh;   // true = override risk to HIGH
};

// ─────────────────────────────────────────────────────────────────────────────
// initModel()
// In stub mode: prints a message and returns false.
// With real model: loads TFLite, allocates tensor arena in PSRAM.
// ─────────────────────────────────────────────────────────────────────────────
bool initModel() {

#ifdef USING_STUB_MODEL
  Serial.println("[AI] Stub model active — no object detection yet");
  Serial.println("[AI] Sensor fusion (ultrasonic + IR) still working");
  return false;
#endif

  // Real model init goes here — added in Week 9
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// runInference()
// In stub mode: always returns "unknown" with zero confidence.
// With real model: runs TFLite on the 96×96 grayscale frame.
// ─────────────────────────────────────────────────────────────────────────────
InferenceResult runInference(const uint8_t* frame_data) {
  InferenceResult result = {
    OBJ_UNKNOWN,          // classIndex
    0,                    // confidence
    OBJ_LABELS[OBJ_UNKNOWN], // label = "unknown"
    false                 // forceHigh
  };

#ifdef USING_STUB_MODEL
  return result;  // stub always returns unknown
#endif

  // Real inference goes here — added in Week 9
  return result;
}