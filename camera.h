/*
 * camera.h — OV2640 init and 96×96 grayscale frame capture
 * ESP32-S3 version — uses PSRAM for frame buffer
 */

#pragma once
#include <Arduino.h>
#include "esp_camera.h"

// ─────────────────────────────────────────────────────────────────────────────
// PIN DEFINITIONS
// These are fixed by the AI Thinker ESP32-CAM hardware layout.
// Do NOT change these — they are hardwired on the PCB.
// ─────────────────────────────────────────────────────────────────────────────
#define CAM_PIN_PWDN     32
#define CAM_PIN_RESET    -1    // not connected on AI Thinker
#define CAM_PIN_XCLK      0
#define CAM_PIN_SIOD     26   // I2C data
#define CAM_PIN_SIOC     27   // I2C clock
#define CAM_PIN_D7       35
#define CAM_PIN_D6       34
#define CAM_PIN_D5       39
#define CAM_PIN_D4       36
#define CAM_PIN_D3       21
#define CAM_PIN_D2       19
#define CAM_PIN_D1       18
#define CAM_PIN_D0        5
#define CAM_PIN_VSYNC    25
#define CAM_PIN_HREF     23
#define CAM_PIN_PCLK     22

// ─────────────────────────────────────────────────────────────────────────────
// FRAME SETTINGS
// 96×96 grayscale is the exact input size our TFLite model expects.
// GRAYSCALE = 1 byte per pixel (vs RGB888 = 3 bytes, JPEG = variable)
// Total frame size = 96 × 96 × 1 = 9,216 bytes — tiny and fast
// ─────────────────────────────────────────────────────────────────────────────
#define CAM_WIDTH   96
#define CAM_HEIGHT  96

// Persistent frame buffer — allocated once in PSRAM, reused every capture
// We never free this — it lives for the life of the program
static uint8_t* grayscale_frame = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// initCamera()
// Configures and starts the OV2640 camera.
// Returns true on success, false on failure.
// Call once in setup() before anything else that uses the camera.
// ─────────────────────────────────────────────────────────────────────────────
bool initCamera() {
  camera_config_t config;

  // ── Hardware pins ──────────────────────────────────────────────────────────
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = CAM_PIN_D0;
  config.pin_d1       = CAM_PIN_D1;
  config.pin_d2       = CAM_PIN_D2;
  config.pin_d3       = CAM_PIN_D3;
  config.pin_d4       = CAM_PIN_D4;
  config.pin_d5       = CAM_PIN_D5;
  config.pin_d6       = CAM_PIN_D6;
  config.pin_d7       = CAM_PIN_D7;
  config.pin_xclk     = CAM_PIN_XCLK;
  config.pin_pclk     = CAM_PIN_PCLK;
  config.pin_vsync    = CAM_PIN_VSYNC;
  config.pin_href     = CAM_PIN_HREF;
  config.pin_sscb_sda = CAM_PIN_SIOD;
  config.pin_sscb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;

  // ── Format settings ────────────────────────────────────────────────────────
  // XCLK: 20MHz is standard for OV2640
  config.xclk_freq_hz  = 20000000;

  // GRAYSCALE = 8-bit single channel — exactly what TFLite model needs
  // Do NOT use PIXFORMAT_JPEG (compressed) or PIXFORMAT_RGB888 (3 channels)
  config.pixel_format  = PIXFORMAT_GRAYSCALE;

  // 96×96 — smallest standard frame size, fastest capture, right size for AI
  config.frame_size    = FRAMESIZE_96X96;

  // jpeg_quality only matters for JPEG format — ignored for grayscale
  config.jpeg_quality  = 12;

  // Single frame buffer — enough for our use case
  // Use 2 for double-buffering if you need faster capture later
  config.fb_count      = 1;

  // Store frame buffer in PSRAM (4MB available on ESP32-CAM)
  // PSRAM is much larger than the 320KB internal SRAM
  config.fb_location   = CAMERA_FB_IN_PSRAM;

  // GRAB_LATEST discards stale frames and always gives the newest one
  // Important for real-time obstacle detection
  config.grab_mode     = CAMERA_GRAB_LATEST;

  // ── Initialize ────────────────────────────────────────────────────────────
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init FAILED — error 0x%x\n", err);
    Serial.println("[CAM] Check: is PSRAM enabled in Tools menu?");
    return false;
  }

  // ── Sensor tuning ──────────────────────────────────────────────────────────
  // These settings help with indoor / low-light performance
  sensor_t* s = esp_camera_sensor_get();
  if (s == nullptr) {
    Serial.println("[CAM] Could not get sensor handle for tuning");
    return false;
  }

  s->set_brightness(s,  1);   // +1 brightness (range: -2 to +2)
  s->set_contrast(s,    1);   // +1 contrast   (range: -2 to +2)
  s->set_saturation(s, -2);   // -2 saturation (grayscale anyway, doesn't matter)
  s->set_whitebal(s,    1);   // auto white balance ON
  s->set_exposure_ctrl(s, 1); // auto exposure ON
  s->set_gain_ctrl(s,   1);   // auto gain ON
  s->set_aec2(s,        1);   // AEC DSP ON (better auto exposure algorithm)

  // ── Allocate persistent grayscale buffer in PSRAM ─────────────────────────
  // We allocate once here and reuse the same buffer every frame capture.
  // ps_malloc() = malloc that specifically uses PSRAM
  grayscale_frame = (uint8_t*)ps_malloc(CAM_WIDTH * CAM_HEIGHT);
  if (grayscale_frame == nullptr) {
    Serial.println("[CAM] FAILED to allocate grayscale buffer in PSRAM");
    Serial.println("[CAM] Check: is PSRAM enabled in Tools menu?");
    return false;
  }

  Serial.println("[CAM] Camera Ready");
  Serial.printf("[CAM] Frame: %dx%d GRAYSCALE = %d bytes per frame\n",
                CAM_WIDTH, CAM_HEIGHT, CAM_WIDTH * CAM_HEIGHT);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// captureGrayscaleFrame()
// Captures one 96×96 grayscale frame from the camera.
// Returns pointer to the frame buffer, or nullptr on failure.
//
// IMPORTANT: Do NOT free() this pointer — it points to our persistent buffer.
// The data is valid until the next call to captureGrayscaleFrame().
// ─────────────────────────────────────────────────────────────────────────────
const uint8_t* captureGrayscaleFrame() {
  // Get a frame from the camera driver
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    Serial.println("[CAM] Frame capture failed — fb is null");
    return nullptr;
  }

  // Validate frame dimensions
  // If these don't match, something is wrong with the camera config
  if (fb->width != CAM_WIDTH || fb->height != CAM_HEIGHT) {
    Serial.printf("[CAM] Wrong frame size: got %dx%d, expected %dx%d\n",
                  fb->width, fb->height, CAM_WIDTH, CAM_HEIGHT);
    esp_camera_fb_return(fb);
    return nullptr;
  }

  // Validate byte count
  // GRAYSCALE = 1 byte per pixel, so total = width × height
  size_t expected = CAM_WIDTH * CAM_HEIGHT;
  if (fb->len != expected) {
    Serial.printf("[CAM] Wrong frame length: got %d, expected %d\n",
                  fb->len, expected);
    esp_camera_fb_return(fb);
    return nullptr;
  }

  // Copy frame into our persistent buffer
  // We copy because fb->buf belongs to the camera driver —
  // we must return it immediately so the driver can reuse it
  memcpy(grayscale_frame, fb->buf, fb->len);

  // MUST call this or the camera driver runs out of buffers and freezes
  esp_camera_fb_return(fb);

  return grayscale_frame;
}

// ─────────────────────────────────────────────────────────────────────────────
// deinitCamera()
// Only needed if going to deep sleep. Not required for normal operation.
// ─────────────────────────────────────────────────────────────────────────────
void deinitCamera() {
  esp_camera_deinit();
  Serial.println("[CAM] Camera deinitialized");
}