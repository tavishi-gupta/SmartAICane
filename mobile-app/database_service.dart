import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:path/path.dart';
import 'package:path_provider/path_provider.dart';
import 'package:sqflite/sqflite.dart';
import '../models/models.dart';

class DatabaseService extends ChangeNotifier {
  static const _dbName    = 'smartguide_v2.db';
  static const _dbVersion = 2;   // bumped from v1 for new columns
  Database? _db;

  Future<void> init() async {
    final dir  = await getDatabasesPath();
    final path = join(dir, _dbName);

    _db = await openDatabase(
      path,
      version: _dbVersion,
      onCreate: _createSchema,
      onUpgrade: _migrateSchema,
    );
    debugPrint('[DB] v2 initialized');
  }

  Future<void> _createSchema(Database db, int version) async {
    await db.execute('''
      CREATE TABLE alert_events (
        id             TEXT PRIMARY KEY,
        time           TEXT NOT NULL,
        distance_cm    INTEGER,
        risk           TEXT,
        object         TEXT DEFAULT 'unknown',
        confidence     INTEGER DEFAULT 0,
        ir_triggered   INTEGER DEFAULT 0,
        ai_active      INTEGER DEFAULT 0,
        false_positive INTEGER DEFAULT 0,
        false_negative INTEGER DEFAULT 0,
        session_id     TEXT
      )
    ''');
    await db.execute('''
      CREATE TABLE sessions (
        id             TEXT PRIMARY KEY,
        start_time     TEXT NOT NULL,
        end_time       TEXT,
        location_type  TEXT DEFAULT 'indoor',
        notes          TEXT DEFAULT ''
      )
    ''');
  }

  // Migrate v1 schema (no confidence/ai_active columns) to v2
  Future<void> _migrateSchema(Database db, int oldV, int newV) async {
    if (oldV < 2) {
      await db.execute('ALTER TABLE alert_events ADD COLUMN confidence INTEGER DEFAULT 0');
      await db.execute('ALTER TABLE alert_events ADD COLUMN ai_active INTEGER DEFAULT 0');
      debugPrint('[DB] Migrated v1 → v2');
    }
  }

  // ── Alert Events ────────────────────────────────────────────────────────────

  Future<void> insertEvent(AlertEvent event, String sessionId) async {
    final map = event.toMap()..['session_id'] = sessionId;
    await _db?.insert('alert_events', map,
        conflictAlgorithm: ConflictAlgorithm.replace);
  }

  Future<void> updateEvent(AlertEvent event) async {
    await _db?.update('alert_events', event.toMap(),
        where: 'id = ?', whereArgs: [event.id]);
    notifyListeners();
  }

  Future<List<AlertEvent>> getAllEvents() async {
    final rows = await _db?.query('alert_events', orderBy: 'time DESC') ?? [];
    return rows.map(AlertEvent.fromMap).toList();
  }

  Future<List<AlertEvent>> getEventsForSession(String sessionId) async {
    final rows = await _db?.query('alert_events',
        where: 'session_id = ?', whereArgs: [sessionId],
        orderBy: 'time DESC') ?? [];
    return rows.map(AlertEvent.fromMap).toList();
  }

  // ── Sessions ────────────────────────────────────────────────────────────────

  Future<void> insertSession(Session session) async {
    await _db?.insert('sessions', {
      'id':            session.id,
      'start_time':    session.startTime.toIso8601String(),
      'end_time':      session.endTime?.toIso8601String(),
      'location_type': session.locationType,
      'notes':         session.notes,
    });
  }

  Future<void> endSession(Session session) async {
    await _db?.update('sessions', {
      'end_time':      session.endTime?.toIso8601String(),
      'location_type': session.locationType,
      'notes':         session.notes,
    }, where: 'id = ?', whereArgs: [session.id]);
  }

  // ── Stats ────────────────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> getStats() async {
    final events   = await getAllEvents();
    final sessions = await _db?.query('sessions') ?? [];
    if (events.isEmpty) return {};

    final objectCounts    = <String, int>{};
    final confByObject    = <String, List<int>>{};
    int highCount = 0, aiCount = 0;

    for (final e in events) {
      if (e.risk == 'HIGH') highCount++;
      if (e.object != 'unknown') {
        aiCount++;
        objectCounts[e.object] = (objectCounts[e.object] ?? 0) + 1;
        confByObject.putIfAbsent(e.object, () => []).add(e.confidence);
      }
    }

    final avgConfByObj = confByObject.map((k, v) =>
      MapEntry(k, (v.reduce((a, b) => a + b) / v.length / 255 * 100).round()));

    return {
      'totalAlerts':   events.length,
      'highAlerts':    highCount,
      'aiDetections':  aiCount,
      'totalSessions': sessions.length,
      'objectCounts':  objectCounts,
      'avgConfByObj':  avgConfByObj,
      'avgDistance':   events.map((e) => e.distanceCm)
                             .reduce((a, b) => a + b) / events.length,
    };
  }

  // ── CSV Export ───────────────────────────────────────────────────────────────

  Future<String> exportCsv() async {
    final events = await getAllEvents();
    final buffer = StringBuffer()
      ..writeln(AlertEvent.csvHeader);
    for (final e in events) {
      buffer.writeln(e.csvRow);
    }
    final dir = await getApplicationDocumentsDirectory();
    final ts  = DateTime.now().toIso8601String()
                  .replaceAll(':', '-').substring(0, 19);
    final file = File(join(dir.path, 'smartguide_v2_$ts.csv'));
    await file.writeAsString(buffer.toString());
    return file.path;
  }
}
