/*
 * SmartGuide Cane v2 — obstacle_model.h
 * ─────────────────────────────────────────────────────────────────────────────
 * TensorFlow Lite Micro inference wrapper.
 *
 * MODEL DETAILS
 * ─────────────────────────────────────────────────────────────────────────────
 * Architecture : MobileNetV1 0.25 depth multiplier (tiny variant)
 * Input        : 96×96×1 grayscale, INT8 quantized [-128, 127]
 * Output       : 8 class scores, INT8
 * Size on flash: ~300KB (fits in ESP32-CAM's 4MB flash alongside firmware)
 * Inference    : ~120–180ms on ESP32 @ 240MHz (with PSRAM tensor arena)
 *
 * HOW TO GET THE MODEL FILE (obstacle_model_data.h)
 * ─────────────────────────────────────────────────────────────────────────────
 * Option A — Use pre-trained model (fastest for prototyping):
 *   1. Download person_detect_model_data.cc from TF Lite Micro examples:
 *      https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/person_detection
 *   2. Rename to obstacle_model_data.h
 *   3. Run the Python training script in ai-model/training/ to fine-tune
 *      on all 8 obstacle classes and export new model data array
 *
 * Option B — Train your own (best accuracy):
 *   Run: python ai-model/training/train_obstacle_model.py
 *   This produces obstacle_model_data.h ready to include here.
 *
 * PLACEHOLDER: Until your model is trained, a stub array is used that
 * always returns "unknown" — the distance + IR sensors still work fully.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

#include <Arduino.h>
#include "config.h"

// TFLite Micro headers (install via Arduino Library Manager:
//   "TensorFlowLite_ESP32" by TensorFlow)
#include "TensorFlowLite_ESP32.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/schema/schema_generated.h"

// ── Model data array ──────────────────────────────────────────────────────────
// If obstacle_model_data.h exists (from training), include it.
// Otherwise use the stub below.
#if __has_include("obstacle_model_data.h")
  #include "obstacle_model_data.h"
#else
  // Stub model — inference always returns class 0 (unknown) until real model added
  static const unsigned char g_obstacle_model_data[] = {};
  static const int g_obstacle_model_data_len = 0;
  #define USING_STUB_MODEL 1
#endif

// ── Globals ───────────────────────────────────────────────────────────────────
namespace {
  const tflite::Model*          model       = nullptr;
  tflite::MicroInterpreter*     interpreter = nullptr;
  TfLiteTensor*                 input       = nullptr;
  TfLiteTensor*                 output      = nullptr;

  // Tensor arena lives in PSRAM (ESP32-CAM has 4MB)
  // Using PSRAM avoids eating into the 320KB internal SRAM
  uint8_t* tensor_arena = nullptr;
}

struct InferenceResult {
  int    classIndex;     // 0–7, index into OBJ_LABELS
  int    confidence;     // 0–255 (INT8 model raw score, shifted to unsigned)
  const char* label;     // pointer to OBJ_LABELS[classIndex]
  bool   forceHigh;      // true if this object overrides risk to HIGH
};

// ── Model init ────────────────────────────────────────────────────────────────
bool initModel() {
#ifdef USING_STUB_MODEL
  Serial.println("[AI] Stub model active — train real model for object detection");
  return false;
#endif

  // Allocate tensor arena in PSRAM
  tensor_arena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
  if (!tensor_arena) {
    Serial.println("[AI] ERROR: Failed to allocate PSRAM tensor arena");
    return false;
  }

  model = tflite::GetModel(g_obstacle_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[AI] ERROR: Model schema version mismatch (%d vs %d)\n",
      model->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }

  // AllOpsResolver includes all ops — for production, use MicroMutableOpResolver
  // with only the ops your model needs (reduces flash usage by ~50KB)
  static tflite::AllOpsResolver resolver;

  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("[AI] ERROR: AllocateTensors failed");
    return false;
  }

  input  = interpreter->input(0);
  output = interpreter->output(0);

  Serial.printf("[AI] Model ready. Arena used: %d bytes / %d KB\n",
    interpreter->arena_used_bytes(), TENSOR_ARENA_KB);
  Serial.printf("[AI] Input shape: %dx%dx%d\n",
    input->dims->data[1], input->dims->data[2], input->dims->data[3]);
  Serial.printf("[AI] Output classes: %d\n", output->dims->data[1]);

  return true;
}

// ── Run inference on a 96×96 grayscale frame ─────────────────────────────────
// frame_data: pointer to CAM_WIDTH * CAM_HEIGHT bytes of grayscale pixel data
InferenceResult runInference(const uint8_t* frame_data) {
  InferenceResult result = {
    .classIndex = OBJ_UNKNOWN,
    .confidence = 0,
    .label      = OBJ_LABELS[OBJ_UNKNOWN],
    .forceHigh  = false,
  };

#ifdef USING_STUB_MODEL
  return result;
#endif

  if (!interpreter || !input || !output) return result;

  // Copy + quantize frame into input tensor
  // Model expects INT8: pixel (0–255) → INT8 via zero_point/scale
  // For INT8 symmetric: map [0,255] → [-128, 127]
  const float input_scale      = input->params.scale;
  const int   input_zero_point = input->params.zero_point;

  for (int i = 0; i < CAM_WIDTH * CAM_HEIGHT; i++) {
    float normalized = (frame_data[i] / 255.0f - 0.5f) / input_scale + input_zero_point;
    input->data.int8[i] = (int8_t)constrain((int)normalized, -128, 127);
  }

  // Run inference
  unsigned long t0 = millis();
  TfLiteStatus invoke_status = interpreter->Invoke();
  unsigned long inferenceMs  = millis() - t0;

  if (invoke_status != kTfLiteOk) {
    Serial.println("[AI] ERROR: Invoke failed");
    return result;
  }

  // Find argmax of output tensor
  int   bestClass = 0;
  int8_t bestScore = output->data.int8[0];

  for (int i = 1; i < NUM_CLASSES; i++) {
    if (output->data.int8[i] > bestScore) {
      bestScore  = output->data.int8[i];
      bestClass  = i;
    }
  }

  // Convert INT8 score [-128,127] to unsigned [0,255] for threshold comparison
  int unsignedScore = (int)bestScore + 128;

  Serial.printf("[AI] %dms → %s (raw=%d unsigned=%d)\n",
    inferenceMs, OBJ_LABELS[bestClass], bestScore, unsignedScore);

  // Only report if above confidence threshold
  if (unsignedScore >= AI_CONFIDENCE_THRESHOLD) {
    result.classIndex = bestClass;
    result.confidence = unsignedScore;
    result.label      = OBJ_LABELS[bestClass];
    result.forceHigh  = OBJ_FORCE_HIGH[bestClass];
  }

  return result;
}
