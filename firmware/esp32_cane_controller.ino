/*
 * SmartGuide Cane v2 — esp32_cane_controller.ino
 * ─────────────────────────────────────────────────────────────────────────────
 * Hardware: ESP32-CAM (AI Thinker module)
 *   - OV2640 camera       → 96×96 grayscale frames → TFLite Micro inference
 *   - HC-SR04 ultrasonic  → TRIG GPIO 12, ECHO GPIO 13
 *   - IR obstacle sensor  → GPIO 15 (active LOW)
 *   - Vibration motor     → GPIO 14 (NPN transistor)
 *   - Passive buzzer      → GPIO 2
 *   - Status LED          → GPIO 33
 *
 * ARCHITECTURE
 * ─────────────────────────────────────────────────────────────────────────────
 *   20Hz sensor loop:
 *     Every loop  → read ultrasonic + IR → EMA smooth → classify distance risk
 *     Every 5th   → capture camera frame → run TFLite inference → get object label
 *     Fuse result → if AI detected stairs/car → force HIGH risk
 *     Haptic      → vibration + buzzer (non-blocking pattern player)
 *     BLE         → send JSON payload every 250ms
 *
 * UPDATED JSON PAYLOAD (v2):
 *   {
 *     "distance_cm": 42,
 *     "risk": "HIGH",
 *     "object": "stairs",      ← now filled by on-device AI
 *     "confidence": 187,       ← AI confidence 0–255
 *     "ir": true,
 *     "ai_active": true,       ← tells app whether AI model is loaded
 *     "ts": 123456
 *   }
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * ARDUINO BOARD: AI Thinker ESP32-CAM
 * PARTITION:     Huge APP (3MB No OTA / 1MB SPIFFS)  ← required for TFLite
 * PSRAM:         Enabled
 * CPU FREQ:      240MHz
 * FLASH FREQ:    80MHz
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "config.h"
#include "camera.h"
#include "obstacle_model.h"

// ── Vibration patterns (ms on/off, 0 = end) ──────────────────────────────────
const int VIB_LOW[]      = {100, 400, 0};
const int VIB_MEDIUM[]   = {150, 200, 150, 400, 0};
const int VIB_HIGH[]     = {200, 100, 200, 100, 200, 300, 0};
// Special pattern for AI-detected critical objects (stairs / car)
const int VIB_CRITICAL[] = {80, 60, 80, 60, 80, 60, 300, 200, 300, 400, 0};

// ── BLE globals ───────────────────────────────────────────────────────────────
BLEServer*          pServer         = nullptr;
BLECharacteristic*  pCharacteristic = nullptr;
bool                deviceConnected = false;
bool                oldConnected    = false;

// ── State ─────────────────────────────────────────────────────────────────────
unsigned long lastSendTime    = 0;
unsigned long lastVibTime     = 0;
float         smoothedDist    = DIST_MAX;
String        lastRisk        = "NONE";
bool          aiModelReady    = false;

// AI result — updated every AI_INFERENCE_EVERY_N_LOOPS iterations
InferenceResult lastInference = {OBJ_UNKNOWN, 0, "unknown", false};
int             loopCounter   = 0;

// ── BLE callbacks ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    deviceConnected = true;
    digitalWrite(PIN_LED, HIGH);
    Serial.println("[BLE] Phone connected");
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
    digitalWrite(PIN_LED, LOW);
    Serial.println("[BLE] Phone disconnected");
  }
};

// ── Sensor functions ──────────────────────────────────────────────────────────

float readUltrasonic() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000);
  if (dur == 0) return DIST_MAX;
  return constrain((dur * 0.034f) / 2.0f, 0, DIST_MAX);
}

bool readIR() {
  return digitalRead(PIN_IR) == LOW;
}

// ── Risk fusion: combines distance sensor + AI object result ──────────────────
String fuseRisk(float dist, bool ir, const InferenceResult& ai) {
  // AI detected a critical object → always HIGH regardless of distance
  if (ai.forceHigh && ai.confidence >= AI_CONFIDENCE_THRESHOLD) {
    return "HIGH";
  }

  // Distance-based classification (same as v1)
  String distRisk = "NONE";
  if (ir || dist < THRESH_HIGH)       distRisk = "HIGH";
  else if (dist < THRESH_MEDIUM)      distRisk = "MEDIUM";
  else if (dist < THRESH_LOW)         distRisk = "LOW";

  // AI detected a person at medium or closer → upgrade to at least MEDIUM
  if (ai.classIndex == OBJ_PERSON &&
      ai.confidence >= AI_CONFIDENCE_THRESHOLD &&
      distRisk == "LOW") {
    distRisk = "MEDIUM";
  }

  return distRisk;
}

// ── Haptic output ─────────────────────────────────────────────────────────────

// Non-blocking vibration pattern player — call every loop tick
void tickVibration() {
  static int            step       = 0;
  static bool           isOn       = false;
  static unsigned long  tLast      = 0;
  static const int*     current    = nullptr;
  static int            currentLen = 0;

  if (current == nullptr || step >= currentLen) {
    if (isOn) { digitalWrite(PIN_VIBRATION, LOW); isOn = false; }
    return;
  }
  if (millis() - tLast >= (unsigned long)current[step]) {
    step++;
    if (step >= currentLen) {
      digitalWrite(PIN_VIBRATION, LOW);
      current = nullptr;
      return;
    }
    isOn = !isOn;
    digitalWrite(PIN_VIBRATION, isOn ? HIGH : LOW);
    tLast = millis();
  }
}

// Start a new pattern (call once to begin; tickVibration handles the rest)
void startVibration(const int* pattern, int len) {
  // Access statics via a pointer trick by calling with nullptr first to reset
  // We use a separate setter function to avoid exposing statics
  static int*           s_current    = nullptr;
  static int            s_currentLen = 0;
  static int            s_step       = 0;
  static bool           s_isOn       = false;
  static unsigned long  s_tLast      = 0;
  // Re-implemented inline — see tickVibration above for the actual running logic
  // This is a thin shim that primes the state machine
  digitalWrite(PIN_VIBRATION, HIGH);
}

// Unified haptic trigger — call when risk level changes
void triggerHaptic(const String& risk, bool aiCritical) {
  if (risk == lastRisk) return;
  if (millis() - lastVibTime < VIB_COOLDOWN_MS) return;
  lastVibTime = millis();

  if (aiCritical && risk == "HIGH") {
    // Distinctive rapid pattern for AI-detected critical objects
    playPattern(VIB_CRITICAL, sizeof(VIB_CRITICAL)/sizeof(int));
    tone(PIN_BUZZER, BUZZ_HIGH_FREQ, 150); delay(200);
    tone(PIN_BUZZER, BUZZ_HIGH_FREQ, 150);
    Serial.println("[ALERT] CRITICAL AI OBJECT — rapid pattern");
  } else if (risk == "HIGH") {
    playPattern(VIB_HIGH, sizeof(VIB_HIGH)/sizeof(int));
    tone(PIN_BUZZER, BUZZ_HIGH_FREQ, BUZZ_HIGH_DUR);
    Serial.println("[ALERT] HIGH");
  } else if (risk == "MEDIUM") {
    playPattern(VIB_MEDIUM, sizeof(VIB_MEDIUM)/sizeof(int));
    tone(PIN_BUZZER, BUZZ_MED_FREQ, BUZZ_MED_DUR);
    Serial.println("[ALERT] MEDIUM");
  } else if (risk == "LOW") {
    playPattern(VIB_LOW, sizeof(VIB_LOW)/sizeof(int));
    Serial.println("[ALERT] LOW");
  } else {
    digitalWrite(PIN_VIBRATION, LOW);
    noTone(PIN_BUZZER);
  }
}

// Non-blocking pattern state machine
namespace VibState {
  const int*   pattern    = nullptr;
  int          len        = 0;
  int          step       = 0;
  bool         on         = false;
  unsigned long tLast     = 0;
}

void playPattern(const int* pattern, int len) {
  VibState::pattern = pattern;
  VibState::len     = len;
  VibState::step    = 0;
  VibState::on      = true;
  VibState::tLast   = millis();
  digitalWrite(PIN_VIBRATION, HIGH);
}

void tickPattern() {
  if (!VibState::pattern || VibState::step >= VibState::len) {
    if (VibState::on) { digitalWrite(PIN_VIBRATION, LOW); VibState::on = false; }
    return;
  }
  if (millis() - VibState::tLast >= (unsigned long)VibState::pattern[VibState::step]) {
    VibState::step++;
    if (VibState::step >= VibState::len) {
      digitalWrite(PIN_VIBRATION, LOW);
      VibState::pattern = nullptr;
      return;
    }
    VibState::on = !VibState::on;
    digitalWrite(PIN_VIBRATION, VibState::on ? HIGH : LOW);
    VibState::tLast = millis();
  }
}

// ── BLE payload ───────────────────────────────────────────────────────────────
String buildPayload(float dist, const String& risk,
                    bool ir, const InferenceResult& ai) {
  String json = "{";
  json += "\"distance_cm\":"  + String((int)dist)                     + ",";
  json += "\"risk\":\""       + risk                                   + "\",";
  json += "\"object\":\""     + String(ai.confidence >= AI_CONFIDENCE_THRESHOLD
                                        ? ai.label : "unknown")        + "\",";
  json += "\"confidence\":"   + String(ai.confidence)                  + ",";
  json += "\"ir\":"           + String(ir ? "true" : "false")          + ",";
  json += "\"ai_active\":"    + String(aiModelReady ? "true" : "false")+ ",";
  json += "\"ts\":"           + String(millis());
  json += "}";
  return json;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n╔══════════════════════════════╗");
  Serial.println("║  SmartGuide Cane v2 (AI)     ║");
  Serial.println("╚══════════════════════════════╝");

  // GPIO setup
  pinMode(PIN_TRIG,      OUTPUT);
  pinMode(PIN_ECHO,      INPUT);
  pinMode(PIN_IR,        INPUT);
  pinMode(PIN_VIBRATION, OUTPUT);
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED,       OUTPUT);

  // Hardware self-test
  Serial.println("[BOOT] Hardware self-test...");
  digitalWrite(PIN_VIBRATION, HIGH); delay(150); digitalWrite(PIN_VIBRATION, LOW);
  tone(PIN_BUZZER, 1000, 100); delay(150);
  tone(PIN_BUZZER, 1500, 100); delay(150);
  tone(PIN_BUZZER, 2000, 100); delay(150);
  Serial.println("[BOOT] Self-test OK");

  // Init camera
  Serial.println("[BOOT] Initializing camera...");
  if (initCamera()) {
    Serial.println("[BOOT] Camera OK");
  } else {
    Serial.println("[BOOT] Camera FAILED — continuing without AI");
    // Three short beeps = camera fail indicator
    for (int i = 0; i < 3; i++) {
      tone(PIN_BUZZER, 500, 80); delay(120);
    }
  }

  // Init TFLite model
  Serial.println("[BOOT] Loading TFLite model...");
  aiModelReady = initModel();
  if (aiModelReady) {
    Serial.println("[BOOT] AI model loaded OK");
    // Two rising beeps = AI ready
    tone(PIN_BUZZER, 1200, 120); delay(180);
    tone(PIN_BUZZER, 2000, 180);
  } else {
    Serial.println("[BOOT] AI model not loaded — distance+IR mode only");
  }

  // BLE setup
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pSvc = pServer->createService(SERVICE_UUID);
  pCharacteristic = pSvc->createCharacteristic(
    CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("{}");
  pSvc->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising as '" BLE_DEVICE_NAME "'");
  Serial.println("[BOOT] Ready.\n");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  loopCounter++;

  // ── 1. Read distance sensors ─────────────────────────────────────────────
  float rawDist = readUltrasonic();
  bool  irHit   = readIR();

  // ── 2. EMA smoothing ─────────────────────────────────────────────────────
  smoothedDist = (SMOOTH_ALPHA * rawDist) + ((1.0f - SMOOTH_ALPHA) * smoothedDist);

  // ── 3. AI inference (every N loops) ──────────────────────────────────────
  if (aiModelReady && (loopCounter % AI_INFERENCE_EVERY_N_LOOPS == 0)) {
    const uint8_t* frame = captureGrayscaleFrame();
    if (frame) {
      lastInference = runInference(frame);
    }
  }

  // ── 4. Fuse sensor + AI into final risk level ─────────────────────────────
  String risk = fuseRisk(smoothedDist, irHit, lastInference);

  // ── 5. Haptic alerts ──────────────────────────────────────────────────────
  bool aiCritical = lastInference.forceHigh &&
                    lastInference.confidence >= AI_CONFIDENCE_THRESHOLD;
  triggerHaptic(risk, aiCritical);
  lastRisk = risk;

  // ── 6. Non-blocking vibration tick ───────────────────────────────────────
  tickPattern();

  // ── 7. BLE notify ─────────────────────────────────────────────────────────
  if (deviceConnected && millis() - lastSendTime > SEND_INTERVAL_MS) {
    String payload = buildPayload(smoothedDist, risk, irHit, lastInference);
    pCharacteristic->setValue(payload.c_str());
    pCharacteristic->notify();
    lastSendTime = millis();

    Serial.printf("[DATA] dist=%.1f risk=%s obj=%s conf=%d ir=%s\n",
      smoothedDist, risk.c_str(),
      lastInference.label, lastInference.confidence,
      irHit ? "Y" : "n");
  }

  // ── 8. BLE reconnect handling ─────────────────────────────────────────────
  if (!deviceConnected && oldConnected) {
    delay(500);
    pServer->startAdvertising();
    oldConnected = false;
  }
  if (deviceConnected && !oldConnected) {
    oldConnected = true;
  }

  delay(LOOP_DELAY_MS);
}
