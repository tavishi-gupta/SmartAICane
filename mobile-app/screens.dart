// screens.dart — Log, Research, Settings screens for SmartGuide v2
import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import 'package:provider/provider.dart';
import '../theme/app_theme.dart';
import '../services/database_service.dart';
import '../services/session_manager.dart';
import '../services/alert_service.dart';
import '../models/models.dart';
import '../widgets/widgets.dart';

// ─────────────────────────────────────────────────────────────────────────────
// Log Screen
// ─────────────────────────────────────────────────────────────────────────────
class LogScreen extends StatefulWidget {
  const LogScreen({super.key});
  @override State<LogScreen> createState() => _LogScreenState();
}

class _LogScreenState extends State<LogScreen> {
  List<AlertEvent> _events = [];
  bool _loading = true;

  @override
  void initState() { super.initState(); _load(); }

  Future<void> _load() async {
    final events = await context.read<DatabaseService>().getAllEvents();
    setState(() { _events = events; _loading = false; });
  }

  @override
  Widget build(BuildContext context) {
    final session = context.watch<SessionManager>();
    return Scaffold(
      backgroundColor: AppTheme.bgDeep,
      appBar: _appBar('Alert Log'),
      body: _loading
          ? const Center(child: CircularProgressIndicator(color: AppTheme.accent))
          : _events.isEmpty
              ? _emptyState('No alerts yet.\nStart a session to begin.')
              : ListView.builder(
                  padding: const EdgeInsets.all(20),
                  itemCount: _events.length,
                  itemBuilder: (_, i) => AlertEventRow(
                    event: _events[i],
                    onFlagFP: () async {
                      await session.flagFalsePositive(_events[i]);
                      _load();
                    },
                  ).animate().fadeIn(
                      delay: Duration(milliseconds: i * 35), duration: 300.ms),
                ),
    );
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Research Screen (v2) — includes AI accuracy stats
// ─────────────────────────────────────────────────────────────────────────────
class ResearchScreen extends StatefulWidget {
  const ResearchScreen({super.key});
  @override State<ResearchScreen> createState() => _ResearchScreenState();
}

class _ResearchScreenState extends State<ResearchScreen> {
  Map<String, dynamic> _stats = {};
  bool _loading   = true;
  bool _exporting = false;

  @override
  void initState() { super.initState(); _load(); }

  Future<void> _load() async {
    final s = await context.read<DatabaseService>().getStats();
    setState(() { _stats = s; _loading = false; });
  }

  Future<void> _export() async {
    setState(() => _exporting = true);
    final path = await context.read<DatabaseService>().exportCsv();
    setState(() => _exporting = false);
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('CSV saved:\n$path'),
        duration: const Duration(seconds: 4),
        backgroundColor: AppTheme.bgCard,
      ));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bgDeep,
      appBar: _appBar('Research Dashboard',
          trailing: IconButton(
            icon: _exporting
                ? const SizedBox(
                    width: 18, height: 18,
                    child: CircularProgressIndicator(
                        color: AppTheme.accentAlt, strokeWidth: 2))
                : const Icon(Icons.download_rounded,
                    color: AppTheme.accentAlt, size: 20),
            onPressed: _exporting ? null : _export,
          )),
      body: _loading
          ? const Center(child: CircularProgressIndicator(color: AppTheme.accent))
          : _stats.isEmpty
              ? _emptyState(
                  'No data yet.\nComplete a session to see AI stats.')
              : SingleChildScrollView(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _researchBanner(),
                      const SizedBox(height: 20),
                      SectionHeader(title: 'Aggregate Stats'),
                      const SizedBox(height: 8),
                      _statsGrid(),
                      const SizedBox(height: 20),
                      if ((_stats['objectCounts'] as Map?)?.isNotEmpty == true) ...[
                        SectionHeader(title: 'Object Detection Distribution'),
                        const SizedBox(height: 8),
                        _objectChart(),
                        const SizedBox(height: 20),
                        SectionHeader(title: 'Avg Confidence Per Object'),
                        const SizedBox(height: 8),
                        _confidenceChart(),
                      ],
                    ],
                  ),
                ),
    );
  }

  Widget _researchBanner() {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AppTheme.accentAlt.withOpacity(0.07),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AppTheme.accentAlt.withOpacity(0.25)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text('ON-DEVICE AI — RESEARCH MODE',
              style: TextStyle(color: AppTheme.accentAlt, fontSize: 10,
                  letterSpacing: 1.5, fontWeight: FontWeight.w700)),
          const SizedBox(height: 6),
          const Text(
            '"Can a quantized TFLite model running at 4Hz on ESP32 meaningfully improve obstacle classification beyond distance + IR sensor fusion alone?"',
            style: TextStyle(color: AppTheme.textPrimary, fontSize: 12,
                fontStyle: FontStyle.italic, height: 1.5),
          ),
          const SizedBox(height: 8),
          Text(
            'Model: MobileNetV1-0.25 INT8 · Input: 96×96 grayscale · Classes: 8 · ~120ms/inference',
            style: TextStyle(color: AppTheme.textSecondary.withOpacity(0.7), fontSize: 10),
          ),
        ],
      ),
    );
  }

  Widget _statsGrid() {
    return GridView.count(
      crossAxisCount: 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      crossAxisSpacing: 10,
      mainAxisSpacing: 10,
      childAspectRatio: 2.2,
      children: [
        StatTile(label: 'Total Alerts',
            value: '${_stats['totalAlerts'] ?? 0}',
            icon: Icons.notifications_outlined),
        StatTile(label: 'High Alerts',
            value: '${_stats['highAlerts'] ?? 0}',
            valueColor: AppTheme.riskHigh,
            icon: Icons.warning_rounded),
        StatTile(label: 'AI Detections',
            value: '${_stats['aiDetections'] ?? 0}',
            valueColor: AppTheme.accent,
            icon: Icons.psychology_outlined),
        StatTile(
            label: 'Avg Distance',
            value: _stats['avgDistance'] != null
                ? '${(_stats['avgDistance'] as double).round()} cm'
                : '—',
            icon: Icons.straighten_outlined),
      ],
    );
  }

  Widget _objectChart() {
    final counts = (_stats['objectCounts'] as Map<String, int>?) ?? {};
    if (counts.isEmpty) return const SizedBox.shrink();
    final total  = counts.values.fold(0, (a, b) => a + b);
    final sorted = counts.entries.toList()..sort((a, b) => b.value.compareTo(a.value));

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppTheme.border)),
      child: Column(children: sorted.map((e) {
        final pct = e.value / total;
        return Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Row(children: [
            Text(_objEmoji(e.key), style: const TextStyle(fontSize: 14)),
            const SizedBox(width: 8),
            SizedBox(width: 60,
                child: Text(e.key,
                    style: const TextStyle(color: AppTheme.textSecondary,
                        fontSize: 11, fontWeight: FontWeight.w600))),
            Expanded(child: ClipRRect(
              borderRadius: BorderRadius.circular(3),
              child: LinearProgressIndicator(value: pct,
                  backgroundColor: AppTheme.border,
                  valueColor: const AlwaysStoppedAnimation(AppTheme.accentAlt),
                  minHeight: 8),
            )),
            const SizedBox(width: 8),
            Text('${(pct * 100).round()}%',
                style: const TextStyle(color: AppTheme.accentAlt,
                    fontSize: 11, fontWeight: FontWeight.w700)),
          ]),
        );
      }).toList()),
    );
  }

  Widget _confidenceChart() {
    final conf = (_stats['avgConfByObj'] as Map<String, int>?) ?? {};
    if (conf.isEmpty) return const SizedBox.shrink();
    final sorted = conf.entries.toList()..sort((a, b) => b.value.compareTo(a.value));

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppTheme.border)),
      child: Column(children: sorted.map((e) {
        final pct = e.value / 100.0;
        final barColor = e.value >= 70 ? AppTheme.riskNone
            : e.value >= 50 ? AppTheme.riskMedium : AppTheme.riskHigh;
        return Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Row(children: [
            Text(_objEmoji(e.key), style: const TextStyle(fontSize: 14)),
            const SizedBox(width: 8),
            SizedBox(width: 60,
                child: Text(e.key,
                    style: const TextStyle(color: AppTheme.textSecondary,
                        fontSize: 11, fontWeight: FontWeight.w600))),
            Expanded(child: ClipRRect(
              borderRadius: BorderRadius.circular(3),
              child: LinearProgressIndicator(value: pct.clamp(0.0, 1.0),
                  backgroundColor: AppTheme.border,
                  valueColor: AlwaysStoppedAnimation(barColor),
                  minHeight: 8),
            )),
            const SizedBox(width: 8),
            Text('${e.value}%',
                style: TextStyle(color: barColor,
                    fontSize: 11, fontWeight: FontWeight.w700)),
          ]),
        );
      }).toList()),
    );
  }

  String _objEmoji(String obj) {
    const m = {'person':'🚶','chair':'🪑','stairs':'🪜','door':'🚪',
               'car':'🚗','wall':'🧱','pole':'🪧'};
    return m[obj] ?? '❓';
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings Screen
// ─────────────────────────────────────────────────────────────────────────────
class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final alerts = context.watch<AlertService>();
    return Scaffold(
      backgroundColor: AppTheme.bgDeep,
      appBar: _appBar('Settings'),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          SectionHeader(title: 'Alerts'),
          const SizedBox(height: 8),
          _ToggleTile(title: 'Voice alerts',
              subtitle: 'TTS obstacle announcements',
              value: alerts.voiceEnabled,
              onChanged: (_) => alerts.toggleVoice()),
          _ToggleTile(title: 'Phone vibration',
              subtitle: 'Haptic feedback mirrors cane motor',
              value: alerts.vibrationEnabled,
              onChanged: (_) => alerts.toggleVibration()),
          const SizedBox(height: 20),
          SectionHeader(title: 'AI Model Info'),
          const SizedBox(height: 8),
          _infoCard(),
          const SizedBox(height: 20),
          SectionHeader(title: 'Test'),
          const SizedBox(height: 8),
          _ActionTile(
              title: 'Test voice alert',
              subtitle: 'Play a sample announcement',
              icon: Icons.volume_up_rounded,
              color: AppTheme.accent,
              onTap: alerts.testVoice),
          const SizedBox(height: 28),
          _disclaimer(),
        ]),
      ),
    );
  }

  Widget _infoCard() {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: AppTheme.border)),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        const Text('ON-DEVICE AI (ESP32-CAM)',
            style: TextStyle(color: AppTheme.accent, fontSize: 10,
                letterSpacing: 1.3, fontWeight: FontWeight.w700)),
        const SizedBox(height: 10),
        _infoRow('Architecture', 'MobileNetV1-0.25 INT8'),
        _infoRow('Input',        '96×96 grayscale'),
        _infoRow('Classes',      '8 obstacle types'),
        _infoRow('Model size',   '~300 KB (on ESP32 flash)'),
        _infoRow('Inference',    '~120–180ms @ 240MHz'),
        _infoRow('Rate',         '~4Hz (every 5 sensor loops)'),
        _infoRow('Arena',        '100 KB PSRAM'),
        _infoRow('Confidence',   'Threshold: 55% (140/255)'),
      ]),
    );
  }

  Widget _infoRow(String k, String v) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(children: [
        SizedBox(width: 100, child: Text(k,
            style: const TextStyle(color: AppTheme.textSecondary, fontSize: 11))),
        Expanded(child: Text(v,
            style: const TextStyle(color: AppTheme.textPrimary,
                fontSize: 11, fontWeight: FontWeight.w600))),
      ]),
    );
  }

  Widget _disclaimer() {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: AppTheme.border)),
      child: const Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Icon(Icons.info_outline, size: 14, color: AppTheme.textSecondary),
        SizedBox(width: 8),
        Expanded(child: Text(
          'SmartGuide Cane is a research prototype. Must NOT be used as a primary navigation aid for unsupervised blind users. Always test with sighted users and a spotter first.',
          style: TextStyle(color: AppTheme.textSecondary, fontSize: 10, height: 1.5),
        )),
      ]),
    );
  }
}

class _ToggleTile extends StatelessWidget {
  final String title, subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;
  const _ToggleTile({required this.title, required this.subtitle,
      required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) => Container(
    margin: const EdgeInsets.only(bottom: 8),
    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
    decoration: BoxDecoration(color: AppTheme.bgCard,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppTheme.border)),
    child: Row(children: [
      Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(title, style: const TextStyle(fontSize: 14,
            fontWeight: FontWeight.w600, color: AppTheme.textPrimary)),
        Text(subtitle, style: const TextStyle(fontSize: 11,
            color: AppTheme.textSecondary)),
      ])),
      Switch(value: value, onChanged: onChanged, activeColor: AppTheme.accent),
    ]),
  );
}

class _ActionTile extends StatelessWidget {
  final String title, subtitle;
  final IconData icon;
  final Color color;
  final VoidCallback onTap;
  const _ActionTile({required this.title, required this.subtitle,
      required this.icon, required this.color, required this.onTap});

  @override
  Widget build(BuildContext context) => GestureDetector(
    onTap: onTap,
    child: Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: AppTheme.border)),
      child: Row(children: [
        Container(padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(color: color.withOpacity(0.1),
                borderRadius: BorderRadius.circular(8)),
            child: Icon(icon, color: color, size: 18)),
        const SizedBox(width: 12),
        Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start,
            children: [
          Text(title, style: const TextStyle(fontSize: 14,
              fontWeight: FontWeight.w600, color: AppTheme.textPrimary)),
          Text(subtitle, style: const TextStyle(fontSize: 11,
              color: AppTheme.textSecondary)),
        ])),
        Icon(Icons.chevron_right,
            color: AppTheme.textSecondary.withOpacity(0.4)),
      ]),
    ),
  );
}

// ── Shared helpers ────────────────────────────────────────────────────────────
PreferredSizeWidget _appBar(String title, {Widget? trailing}) => AppBar(
  backgroundColor: AppTheme.bgDeep,
  elevation: 0,
  titleTextStyle: const TextStyle(color: AppTheme.textPrimary,
      fontWeight: FontWeight.w700, fontSize: 18),
  iconTheme: const IconThemeData(color: AppTheme.textPrimary),
  title: Text(title),
  actions: trailing != null ? [trailing] : null,
  bottom: PreferredSize(
    preferredSize: const Size.fromHeight(1),
    child: Container(height: 1, color: AppTheme.border),
  ),
);

Widget _emptyState(String msg) => Center(
  child: Padding(
    padding: const EdgeInsets.all(40),
    child: Text(msg, textAlign: TextAlign.center,
        style: const TextStyle(color: AppTheme.textSecondary,
            fontSize: 14, height: 1.6)),
  ),
);
