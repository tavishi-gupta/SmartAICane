import 'dart:convert';

// ── BLE Payload from ESP32 (v2 — includes on-device AI result) ───────────────
class CaneReading {
  final int    distanceCm;
  final String risk;          // NONE / LOW / MEDIUM / HIGH
  final String object;        // label from ESP32-CAM TFLite model
  final int    confidence;    // 0–255 (INT8 model score, unsigned)
  final bool   irTriggered;
  final bool   aiActive;      // true = ESP32 has a loaded TFLite model
  final int    timestamp;     // millis() from ESP32

  const CaneReading({
    required this.distanceCm,
    required this.risk,
    required this.object,
    required this.confidence,
    required this.irTriggered,
    required this.aiActive,
    required this.timestamp,
  });

  factory CaneReading.empty() => const CaneReading(
    distanceCm:  999,
    risk:        'NONE',
    object:      'unknown',
    confidence:  0,
    irTriggered: false,
    aiActive:    false,
    timestamp:   0,
  );

  factory CaneReading.fromJson(String raw) {
    try {
      final m = jsonDecode(raw) as Map<String, dynamic>;
      return CaneReading(
        distanceCm:  (m['distance_cm'] as num?)?.toInt()  ?? 999,
        risk:        (m['risk']        as String?)         ?? 'NONE',
        object:      (m['object']      as String?)         ?? 'unknown',
        confidence:  (m['confidence']  as num?)?.toInt()  ?? 0,
        irTriggered: (m['ir']          as bool?)           ?? false,
        aiActive:    (m['ai_active']   as bool?)           ?? false,
        timestamp:   (m['ts']          as num?)?.toInt()  ?? 0,
      );
    } catch (_) {
      return CaneReading.empty();
    }
  }

  bool get isDangerous       => risk == 'HIGH' || irTriggered;
  bool get hasObjectDetected => object != 'unknown' && confidence > 0;

  /// Human-readable confidence percentage
  String get confidencePct =>
    confidence > 0 ? '${(confidence / 255 * 100).round()}%' : '—';

  /// Icon for detected object
  String get objectEmoji {
    switch (object) {
      case 'person':  return '🚶';
      case 'chair':   return '🪑';
      case 'stairs':  return '🪜';
      case 'door':    return '🚪';
      case 'car':     return '🚗';
      case 'wall':    return '🧱';
      case 'pole':    return '🪧';
      default:        return '❓';
    }
  }

  /// Whether this object is critical (always HIGH risk)
  bool get isCriticalObject =>
    object == 'stairs' || object == 'car';
}

// ── Alert Event ───────────────────────────────────────────────────────────────
class AlertEvent {
  final String   id;
  final DateTime time;
  final int      distanceCm;
  final String   risk;
  final String   object;
  final int      confidence;
  final bool     irTriggered;
  final bool     aiActive;
  int            falsePositive;
  int            falseNegative;

  AlertEvent({
    required this.id,
    required this.time,
    required this.distanceCm,
    required this.risk,
    required this.object,
    required this.confidence,
    required this.irTriggered,
    required this.aiActive,
    this.falsePositive = 0,
    this.falseNegative = 0,
  });

  Map<String, dynamic> toMap() => {
    'id':            id,
    'time':          time.toIso8601String(),
    'distance_cm':   distanceCm,
    'risk':          risk,
    'object':        object,
    'confidence':    confidence,
    'ir_triggered':  irTriggered ? 1 : 0,
    'ai_active':     aiActive ? 1 : 0,
    'false_positive': falsePositive,
    'false_negative': falseNegative,
  };

  factory AlertEvent.fromMap(Map<String, dynamic> m) => AlertEvent(
    id:            m['id'],
    time:          DateTime.parse(m['time']),
    distanceCm:    m['distance_cm'],
    risk:          m['risk'],
    object:        m['object'] ?? 'unknown',
    confidence:    m['confidence'] ?? 0,
    irTriggered:   m['ir_triggered'] == 1,
    aiActive:      (m['ai_active'] ?? 0) == 1,
    falsePositive: m['false_positive'] ?? 0,
    falseNegative: m['false_negative'] ?? 0,
  );

  String get csvRow =>
    '"$id","$time",$distanceCm,"$risk","$object",$confidence,$irTriggered,$aiActive,$falsePositive,$falseNegative';

  static String get csvHeader =>
    'id,time,distance_cm,risk,object,confidence,ir_triggered,ai_active,false_positive,false_negative';
}

// ── Session ───────────────────────────────────────────────────────────────────
class Session {
  final String           id;
  final DateTime         startTime;
  DateTime?              endTime;
  final List<AlertEvent> events;
  String                 locationType;
  String                 notes;

  Session({
    required this.id,
    required this.startTime,
    this.endTime,
    List<AlertEvent>? events,
    this.locationType = 'indoor',
    this.notes = '',
  }) : events = events ?? [];

  Duration get duration       => (endTime ?? DateTime.now()).difference(startTime);
  int get highAlertCount      => events.where((e) => e.risk == 'HIGH').length;
  int get totalAlerts         => events.length;
  int get aiDetectedCount     => events.where((e) => e.object != 'unknown').length;

  double get avgDistance =>
    events.isEmpty ? 0 :
    events.map((e) => e.distanceCm).reduce((a, b) => a + b) / events.length;

  String get mostCommonObject {
    final meaningful = events.where((e) => e.object != 'unknown').toList();
    if (meaningful.isEmpty) return 'none';
    final counts = <String, int>{};
    for (final e in meaningful) {
      counts[e.object] = (counts[e.object] ?? 0) + 1;
    }
    return counts.entries.reduce((a, b) => a.value > b.value ? a : b).key;
  }

  double get avgConfidence {
    final aiEvents = events.where((e) => e.confidence > 0).toList();
    if (aiEvents.isEmpty) return 0;
    return aiEvents.map((e) => e.confidence).reduce((a, b) => a + b) /
        aiEvents.length / 255 * 100;
  }
}

// ── Voice alert text ──────────────────────────────────────────────────────────
class VoiceAlert {
  static String forReading(CaneReading r) {
    // AI-detected critical objects take top priority
    if (r.object == 'stairs')
      return 'Warning. Stairs detected ahead.';
    if (r.object == 'car')
      return 'Caution. Vehicle detected ahead.';
    if (r.object == 'person' && r.risk == 'HIGH')
      return 'Person very close.';
    if (r.object == 'person' && r.risk == 'MEDIUM')
      return 'Person ahead.';
    if (r.object == 'chair' && r.risk == 'HIGH')
      return 'Chair in path.';
    if (r.object == 'door')
      return 'Door ahead.';
    if (r.object == 'pole')
      return 'Pole detected.';
    if (r.irTriggered)
      return 'Obstacle very close. Stop.';

    switch (r.risk) {
      case 'HIGH':   return 'Obstacle ahead. Stop.';
      case 'MEDIUM': return 'Caution. Object at ${r.distanceCm} centimeters.';
      case 'LOW':    return 'Object nearby.';
      default:       return '';
    }
  }
}
