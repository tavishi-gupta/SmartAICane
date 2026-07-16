#include "config.h"
#include "camera.h"
#include "obstacle_model.h"

// ── Vibration patterns ────────────────────────────────────────────────────────
const int VIB_LOW[]    = {100, 400, 0};
const int VIB_MEDIUM[] = {150, 200, 150, 400, 0};
const int VIB_HIGH[]   = {200, 100, 200, 100, 200, 300, 0};

// ── Global state ──────────────────────────────────────────────────────────────
float         smoothedDistance = DIST_MAX;
String        lastRisk         = "NONE";
unsigned long lastVibTime      = 0;
bool          cameraReady      = false;
bool          modelReady       = false;
int           loopCounter      = 0;

// Vibration state machine
namespace VibState {
  const int*    pattern = nullptr;
  int           len     = 0;
  int           step    = 0;
  bool          motorOn = false;
  unsigned long tLast   = 0;
}

// ── Self-test beeps ───────────────────────────────────────────────────────────
// Plays 3 rising beeps on boot so you know hardware is alive
void selfTestBeeps() {
  Serial.println("[BOOT] Self-test beeps...");
  tone(PIN_BUZZER, 1000, 120); delay(200);
  tone(PIN_BUZZER, 1500, 120); delay(200);
  tone(PIN_BUZZER, 2000, 180); delay(300);
  Serial.println("[BOOT] Self-test OK");
}

// ── readUltrasonic() ──────────────────────────────────────────────────────────
float readUltrasonic() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return DIST_MAX;

  float raw = constrain((duration * 0.034f) / 2.0f, 0, DIST_MAX);
  smoothedDistance = (SMOOTH_ALPHA * raw) +
                     ((1.0f - SMOOTH_ALPHA) * smoothedDistance);
  return smoothedDistance;
}

// ── classifyRisk() ────────────────────────────────────────────────────────────
String classifyRisk(float dist, const InferenceResult& ai) {
  // AI detected something that always forces HIGH
  if (ai.forceHigh && ai.confidence >= 140) return "HIGH";

  bool irHit = (digitalRead(PIN_IR) == LOW);
  if (irHit || dist < THRESH_HIGH)  return "HIGH";
  if (dist < THRESH_MEDIUM)         return "MEDIUM";
  if (dist < THRESH_LOW)            return "LOW";
  return "NONE";
}

// ── Vibration pattern player ──────────────────────────────────────────────────
void playPattern(const int* pattern, int len) {
  VibState::pattern = pattern;
  VibState::len     = len;
  VibState::step    = 0;
  VibState::motorOn = true;
  VibState::tLast   = millis();
  digitalWrite(PIN_VIBRATION, HIGH);
}

void tickPattern() {
  if (VibState::pattern == nullptr || VibState::step >= VibState::len) {
    digitalWrite(PIN_VIBRATION, LOW);
    return;
  }
  if (millis() - VibState::tLast >=
      (unsigned long)VibState::pattern[VibState::step]) {
    VibState::step++;
    if (VibState::step >= VibState::len) {
      digitalWrite(PIN_VIBRATION, LOW);
      VibState::pattern = nullptr;
      return;
    }
    VibState::motorOn = !VibState::motorOn;
    digitalWrite(PIN_VIBRATION, VibState::motorOn ? HIGH : LOW);
    VibState::tLast = millis();
  }
}

// ── triggerAlerts() ───────────────────────────────────────────────────────────
void triggerAlerts(const String& risk) {
  bool changed     = (risk != lastRisk);
  bool cooldownDone = (millis() - lastVibTime >= VIB_COOLDOWN_MS);
  if (!changed && !cooldownDone) return;

  lastRisk    = risk;
  lastVibTime = millis();

  if      (risk == "HIGH")   {
    playPattern(VIB_HIGH, sizeof(VIB_HIGH)/sizeof(int));
    tone(PIN_BUZZER, BUZZ_HIGH_FREQ, BUZZ_HIGH_DUR);
  }
  else if (risk == "MEDIUM") {
    playPattern(VIB_MEDIUM, sizeof(VIB_MEDIUM)/sizeof(int));
    tone(PIN_BUZZER, BUZZ_MED_FREQ, BUZZ_MED_DUR);
  }
  else if (risk == "LOW")    {
    playPattern(VIB_LOW, sizeof(VIB_LOW)/sizeof(int));
  }
  else {
    VibState::pattern = nullptr;
    digitalWrite(PIN_VIBRATION, LOW);
    noTone(PIN_BUZZER);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// SETUP
// ═════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║   SmartGuide Cane v2 — Boot    ║");
  Serial.println("╚════════════════════════════════╝");

  // Pin setup
  pinMode(PIN_TRIG,      OUTPUT);
  pinMode(PIN_ECHO,      INPUT);
  pinMode(PIN_IR,        INPUT);
  pinMode(PIN_VIBRATION, OUTPUT);

  // Step 1: self-test beeps
  selfTestBeeps();

  // Step 2: camera init
  Serial.println("\n[BOOT] Initializing camera...");
  cameraReady = initCamera();
  if (!cameraReady) {
    Serial.println("[BOOT] Camera FAILED — continuing without vision");
    // Two low beeps = camera fail indicator
    tone(PIN_BUZZER, 500, 100); delay(150);
    tone(PIN_BUZZER, 500, 100); delay(150);
  }

  // Step 3: AI model init
  Serial.println("\n[BOOT] Loading AI model...");
  modelReady = initModel();
  if (modelReady) {
    // Two rising beeps = model loaded
    tone(PIN_BUZZER, 1200, 120); delay(180);
    tone(PIN_BUZZER, 2000, 180);
    Serial.println("[BOOT] AI model loaded OK");
  }

  // Step 4: BLE — placeholder for now
  Serial.println("\n[BOOT] BLE: not yet initialized (Week 5)");

  // Step 5: ready
  Serial.println("\n[BOOT] Boot complete — starting sensor loop");
  Serial.println("──────────────────────────────────────");
  Serial.printf("  Camera:  %s\n", cameraReady ? "READY" : "offline");
  Serial.printf("  AI Model: %s\n", modelReady  ? "LOADED" : "stub mode");
  Serial.println("──────────────────────────────────────\n");
}

// ═════════════════════════════════════════════════════════════════════════════
// LOOP
// ═════════════════════════════════════════════════════════════════════════════
void loop() {
  loopCounter++;

  // 1. Read distance sensor
  float dist = readUltrasonic();

  // 2. Camera + AI inference every 5th loop (~4Hz at 20Hz loop rate)
  InferenceResult aiResult = {OBJ_UNKNOWN, 0, "unknown", false};
  if (cameraReady && (loopCounter % 5 == 0)) {
    const uint8_t* frame = captureGrayscaleFrame();
    if (frame != nullptr) {
      aiResult = runInference(frame);
      // Only print when AI detects something
      if (aiResult.confidence >= 140) {
        Serial.printf("[AI] Detected: %s (confidence %d%%)\n",
                      aiResult.label,
                      (aiResult.confidence * 100) / 255);
      }
    }
  }

  // 3. Classify risk
  String risk = classifyRisk(dist, aiResult);

  // 4. Trigger alerts
  triggerAlerts(risk);

  // 5. Tick vibration pattern
  tickPattern();

  // 6. Serial output
  Serial.printf("dist=%.1fcm  risk=%-6s  obj=%-8s  loop=%d\n",
                dist < DIST_MAX ? dist : 0,
                risk.c_str(),
                aiResult.label,
                loopCounter);

  delay(LOOP_DELAY_MS);
}