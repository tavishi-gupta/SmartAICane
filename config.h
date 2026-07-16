#pragma once

// ── Pins ──────────────────────────────────────────────────────────────────────
#define PIN_TRIG        12    // HC-SR04 trigger
#define PIN_ECHO        13    // HC-SR04 echo
#define PIN_IR          15    // IR obstacle sensor (active LOW)
#define PIN_VIBRATION   14    // Vibration motor via NPN transistor
#define PIN_BUZZER       2    // Passive buzzer

// ── Distance thresholds (centimeters) ────────────────────────────────────────
#define THRESH_HIGH     50    // closer than this = HIGH risk
#define THRESH_MEDIUM  100    // closer than this = MEDIUM risk
#define THRESH_LOW     150    // closer than this = LOW risk
#define DIST_MAX       400    // max sensor range

// ── Sensor tuning ─────────────────────────────────────────────────────────────
#define SMOOTH_ALPHA    0.30f // EMA smoothing — 0=fully smoothed, 1=raw
#define LOOP_DELAY_MS   50    // main loop delay = 20Hz
#define VIB_COOLDOWN_MS 800   // min ms between vibration bursts

// ── Buzzer frequencies ────────────────────────────────────────────────────────
#define BUZZ_HIGH_FREQ  2000  // Hz for HIGH alert
#define BUZZ_MED_FREQ   1200  // Hz for MEDIUM alert
#define BUZZ_HIGH_DUR    400  // ms
#define BUZZ_MED_DUR     200  // ms