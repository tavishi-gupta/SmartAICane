/*
 * SmartGuide Cane v2 — camera.h
 * ─────────────────────────────────────────────────────────────────────────────
 * ESP32-CAM (AI Thinker) camera init and frame capture.
 * Captures 96×96 grayscale frames for TFLite inference.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

#include <Arduino.h>
#include "esp_camera.h"
#include "config.h"

// ── AI Thinker ESP32-CAM pin definition ──────────────────────────────────────
// These are fixed by the hardware — do not change
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1   // not connected
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// ── Shared grayscale frame buffer ─────────────────────────────────────────────
// Allocated once in PSRAM and reused every inference cycle
static uint8_t* grayscale_frame = nullptr;

// ── Camera init ───────────────────────────────────────────────────────────────
bool initCamera() {
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = CAM_PIN_D0;
  config.pin_d1        = CAM_PIN_D1;
  config.pin_d2        = CAM_PIN_D2;
  config.pin_d3        = CAM_PIN_D3;
  config.pin_d4        = CAM_PIN_D4;
  config.pin_d5        = CAM_PIN_D5;
  config.pin_d6        = CAM_PIN_D6;
  config.pin_d7        = CAM_PIN_D7;
  config.pin_xclk      = CAM_PIN_XCLK;
  config.pin_pclk      = CAM_PIN_PCLK;
  config.pin_vsync     = CAM_PIN_VSYNC;
  config.pin_href      = CAM_PIN_HREF;
  config.pin_sscb_sda  = CAM_PIN_SIOD;
  config.pin_sscb_scl  = CAM_PIN_SIOC;
  config.pin_pwdn      = CAM_PIN_PWDN;
  config.pin_reset     = CAM_PIN_RESET;
  config.xclk_freq_hz  = 20000000;  // 20MHz XCLK

  // Capture at 96×96 grayscale — smallest format, fastest transfer, right size for model
  config.pixel_format  = PIXFORMAT_GRAYSCALE;
  config.frame_size    = FRAMESIZE_96X96;
  config.jpeg_quality  = 12;      // not used for grayscale but required by API
  config.fb_count      = 1;       // single frame buffer (2 if PSRAM for double-buffer)
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.grab_mode     = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x\n", err);
    return false;
  }

  // Apply tuning for indoor/low-light performance
  sensor_t* s = esp_camera_sensor_get();
  s->set_brightness(s, 1);      // +1 brightness
  s->set_contrast(s, 1);        // +1 contrast
  s->set_saturation(s, -2);     // -2 saturation (grayscale anyway)
  s->set_special_effect(s, 0);  // no effect
  s->set_whitebal(s, 1);        // auto white balance on
  s->set_exposure_ctrl(s, 1);   // auto exposure on
  s->set_gain_ctrl(s, 1);       // auto gain on
  s->set_aec2(s, 1);            // AEC DSP on

  // Allocate permanent grayscale buffer in PSRAM
  grayscale_frame = (uint8_t*)ps_malloc(CAM_WIDTH * CAM_HEIGHT);
  if (!grayscale_frame) {
    Serial.println("[CAM] ERROR: Failed to allocate grayscale buffer in PSRAM");
    return false;
  }

  Serial.printf("[CAM] Ready. Capture size: %dx%d grayscale\n", CAM_WIDTH, CAM_HEIGHT);
  return true;
}

// ── Capture a single 96×96 grayscale frame ───────────────────────────────────
// Returns pointer to grayscale_frame buffer, or nullptr on failure.
// Caller must NOT free this pointer — it is reused each call.
const uint8_t* captureGrayscaleFrame() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Frame capture failed");
    return nullptr;
  }

  // The GRAYSCALE format gives us raw 8-bit pixels directly
  // fb->len should be CAM_WIDTH * CAM_HEIGHT
  if (fb->len != (size_t)(CAM_WIDTH * CAM_HEIGHT)) {
    Serial.printf("[CAM] Unexpected frame size: %d (expected %d)\n",
      fb->len, CAM_WIDTH * CAM_HEIGHT);
    esp_camera_fb_return(fb);
    return nullptr;
  }

  // Copy into our persistent buffer and release the camera buffer
  memcpy(grayscale_frame, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  return grayscale_frame;
}

// ── Deinit (call if going to deep sleep) ─────────────────────────────────────
void deinitCamera() {
  esp_camera_deinit();
}
