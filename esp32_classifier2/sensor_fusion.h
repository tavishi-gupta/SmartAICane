/*
  sensor_fusion.h / sensor_fusion logic
  --------------------------------------
  Combines the AI classifier with an HC-SR04 ultrasonic sensor and an IR
  obstacle sensor to drive a vibration motor, per this risk model:

    HIGH   : AI detects stairs/car/pedestrian-crosswalk above confidence
             threshold, OR IR triggered, OR distance < 50cm
    MEDIUM : distance is 50-100cm (or AI detects a person while distance
             would otherwise be LOW — upgraded)
    LOW    : distance is 100-150cm
    NONE   : distance > 150cm

  HIGH risk buzzes the motor continuously until the risk clears.
  MEDIUM/LOW use distinct pulse patterns, same as your original sketch.
  Motor timing is non-blocking (uses millis()), so it never stalls the
  camera/web server while pulsing.

  *** IMPORTANT — VERIFY THESE PIN NUMBERS BEFORE WIRING ***
  Your camera already uses GPIOs 4,5,6,7,8,9,10,11,12,13,15,16,17,18
  internally on the ESP32S3_EYE (see camera_pins.h). The pins below are
  placeholders chosen to avoid that list, but you must check them against
  your board's actual pinout diagram/silkscreen before wiring anything —
  some GPIOs on ESP32-S3 boards are reserved for flash/PSRAM or are
  strapping pins and must not be used for general I/O.
*/

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <Arduino.h>

// ---- Pin definitions — VERIFY against your board before wiring ----
const int TRIG_PIN = 42;         // HC-SR04 trigger
const int ECHO_PIN = 41;         // HC-SR04 echo
const int IR_SENSOR_PIN = 2;   // IR obstacle sensor digital out
const int MOTOR_PIN = 47;       // Vibration motor control

// ---- Tunables ----
const float AI_CONFIDENCE_THRESHOLD = 0.5;  // 50% — adjust based on real-world testing
const unsigned long CLASSIFY_INTERVAL_MS = 500;   // how often to run AI inference
const unsigned long SENSOR_INTERVAL_MS = 100;     // how often to read distance/IR

enum RiskLevel { RISK_NONE, RISK_LOW, RISK_MEDIUM, RISK_HIGH };

// Cached latest readings, updated at their own paces (AI is much slower
// than the ultrasonic/IR sensors, so we don't want to wait for a new AI
// result on every single loop iteration).
static String g_lastAILabel = "";
static float g_lastAIConfidence = 0.0;
static unsigned long g_lastClassifyTime = 0;

static float g_lastDistance = -1;
static bool g_lastIRTriggered = false;
static unsigned long g_lastSensorTime = 0;

// Non-blocking motor pulse state
static RiskLevel g_currentRisk = RISK_NONE;
static unsigned long g_motorPulseTimer = 0;
static bool g_motorPulseState = false;

void sensorFusionInit() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
}

// Reads the HC-SR04. Returns distance in cm, or -1 on timeout/no echo.
static float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 30ms timeout (~5m range) so a missing echo can't hang the loop
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return -1;
  }
  return duration * 0.0343f / 2.0f;
}

// Objects that mean immediate danger regardless of distance.
static bool isHighRiskLabel(const String &label) {
  return label == "Stairs" || label == "Car" || label == "Pedestrian crosswalk";
}

static RiskLevel fuseRisk(float distance, bool irTriggered, const String &aiLabel, float aiConfidence) {
  if (isHighRiskLabel(aiLabel) && aiConfidence >= AI_CONFIDENCE_THRESHOLD) {
    return RISK_HIGH;
  }
  if (irTriggered || (distance > 0 && distance < 50)) {
    return RISK_HIGH;
  }

  RiskLevel distanceRisk;
  if (distance >= 50 && distance <= 100) {
    distanceRisk = RISK_MEDIUM;
  } else if (distance > 100 && distance <= 150) {
    distanceRisk = RISK_LOW;
  } else {
    distanceRisk = RISK_NONE;
  }

  // Upgrade LOW to MEDIUM if the AI sees a person at that range.
  if (distanceRisk == RISK_LOW && aiLabel == "Person") {
    return RISK_MEDIUM;
  }

  return distanceRisk;
}

// Drives the motor for the current risk level without blocking the loop.
// Call this every iteration — it manages its own timing internally.
static void updateMotor(RiskLevel risk) {
  unsigned long now = millis();

  switch (risk) {
    case RISK_HIGH:
      // Continuous buzz until risk clears.
      digitalWrite(MOTOR_PIN, HIGH);
      break;

    case RISK_MEDIUM:
      // Pulse: 150ms on, 150ms off.
      if (now - g_motorPulseTimer >= 150) {
        g_motorPulseTimer = now;
        g_motorPulseState = !g_motorPulseState;
        digitalWrite(MOTOR_PIN, g_motorPulseState ? HIGH : LOW);
      }
      break;

    case RISK_LOW:
      // Slow pulse: 100ms on, 400ms off.
      {
        unsigned long interval = g_motorPulseState ? 100 : 400;
        if (now - g_motorPulseTimer >= interval) {
          g_motorPulseTimer = now;
          g_motorPulseState = !g_motorPulseState;
          digitalWrite(MOTOR_PIN, g_motorPulseState ? HIGH : LOW);
        }
      }
      break;

    case RISK_NONE:
    default:
      digitalWrite(MOTOR_PIN, LOW);
      g_motorPulseState = false;
      break;
  }
}

const char *riskLevelName(RiskLevel r) {
  switch (r) {
    case RISK_HIGH: return "HIGH";
    case RISK_MEDIUM: return "MEDIUM";
    case RISK_LOW: return "LOW";
    default: return "NONE";
  }
}

// Call this once per loop() iteration. Internally throttles how often it
// actually reads sensors / runs AI inference, so it's safe to call every
// single loop pass with no extra timing logic needed at the call site.
void sensorFusionUpdate() {
  unsigned long now = millis();

  // ---- Sensors: cheap, read frequently ----
  if (now - g_lastSensorTime >= SENSOR_INTERVAL_MS) {
    g_lastSensorTime = now;
    g_lastDistance = readDistanceCm();
    g_lastIRTriggered = (digitalRead(IR_SENSOR_PIN) == LOW);
  }

  // ---- AI classification: expensive, read less frequently ----
  if (now - g_lastClassifyTime >= CLASSIFY_INTERVAL_MS) {
    g_lastClassifyTime = now;
    String label;
    float confidence;
    if (runClassification(label, confidence)) {
      g_lastAILabel = label;
      g_lastAIConfidence = confidence;
    }
  }

  // ---- Fuse + drive motor ----
  g_currentRisk = fuseRisk(g_lastDistance, g_lastIRTriggered, g_lastAILabel, g_lastAIConfidence);
  updateMotor(g_currentRisk);

  Serial.print("Distance: ");
  Serial.print(g_lastDistance);
  Serial.print(" cm | IR: ");
  Serial.print(g_lastIRTriggered ? "TRIGGERED" : "clear");
  Serial.print(" | AI: ");
  Serial.print(g_lastAILabel);
  Serial.print(" (");
  Serial.print(g_lastAIConfidence * 100);
  Serial.print("%) | Risk: ");
  Serial.println(riskLevelName(g_currentRisk));
}

#endif  // SENSOR_FUSION_H
