import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import 'package:provider/provider.dart';
import '../theme/app_theme.dart';
import '../services/ble_service.dart';
import '../services/session_manager.dart';
import '../services/alert_service.dart';
import '../widgets/widgets.dart';
import 'screens.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final ble     = context.watch<BleService>();
    final session = context.watch<SessionManager>();
    final alerts  = context.watch<AlertService>();
    final reading = ble.lastReading;

    return Scaffold(
      backgroundColor: AppTheme.bgDeep,
      body: SafeArea(
        child: Column(
          children: [
            _buildTopBar(context, ble, alerts),
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.symmetric(horizontal: 20),
                child: Column(
                  children: [
                    const SizedBox(height: 24),

                    // ── Main risk + distance display ──────────────────────
                    RiskDisplay(reading: reading)
                        .animate()
                        .fadeIn(duration: 500.ms)
                        .scale(begin: const Offset(0.95, 0.95)),

                    const SizedBox(height: 16),

                    // ── AI detection card ─────────────────────────────────
                    if (ble.isConnected) _buildAiCard(reading),

                    const SizedBox(height: 20),

                    // ── Live stats row ────────────────────────────────────
                    if (ble.isConnected) _buildLiveStats(session, reading),

                    const SizedBox(height: 20),

                    // ── Start/End session button ──────────────────────────
                    _buildSessionButton(context, session, ble),

                    const SizedBox(height: 20),

                    // ── Live event feed ───────────────────────────────────
                    if (session.isActive &&
                        session.activeSession!.events.isNotEmpty) ...[
                      SectionHeader(
                        title: 'Live Feed',
                        trailing: Text(
                          '${session.activeSession!.events.length} events',
                          style: const TextStyle(
                              color: AppTheme.textSecondary, fontSize: 11),
                        ),
                      ),
                      const SizedBox(height: 8),
                      ...session.activeSession!.events.reversed.take(5).map(
                        (e) => AlertEventRow(
                          event: e,
                          onFlagFP: () => session.flagFalsePositive(e),
                        ),
                      ),
                    ],

                    const SizedBox(height: 20),
                    _buildSecondaryActions(context),
                    const SizedBox(height: 32),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTopBar(BuildContext context, BleService ble, AlertService alerts) {
    return Container(
      padding: const EdgeInsets.fromLTRB(20, 12, 12, 12),
      decoration: BoxDecoration(
          border: Border(bottom: BorderSide(color: AppTheme.border))),
      child: Row(
        children: [
          const Icon(Icons.accessibility_new_rounded,
              color: AppTheme.accent, size: 22),
          const SizedBox(width: 8),
          const Text('SmartGuide',
              style: TextStyle(
                  color: AppTheme.textPrimary,
                  fontSize: 18,
                  fontWeight: FontWeight.w700,
                  letterSpacing: -0.3)),
          const SizedBox(width: 8),
          // v2 badge
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: AppTheme.accent.withOpacity(0.15),
              borderRadius: BorderRadius.circular(4),
              border: Border.all(color: AppTheme.accent.withOpacity(0.3)),
            ),
            child: const Text('AI',
                style: TextStyle(
                    color: AppTheme.accent,
                    fontSize: 9,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 1)),
          ),
          const Spacer(),
          GestureDetector(
            onTap: alerts.toggleVoice,
            child: Padding(
              padding: const EdgeInsets.all(8),
              child: Icon(
                alerts.voiceEnabled
                    ? Icons.volume_up_rounded
                    : Icons.volume_off_rounded,
                color: alerts.voiceEnabled
                    ? AppTheme.accent
                    : AppTheme.textSecondary,
                size: 20,
              ),
            ),
          ),
          ConnectionPill(
            connected: ble.isConnected,
            label: ble.statusLabel,
            onTap: ble.isConnected ? null : () => ble.startScan(),
          ),
          IconButton(
            icon: const Icon(Icons.tune_rounded,
                size: 20, color: AppTheme.textSecondary),
            onPressed: () => Navigator.push(context,
                MaterialPageRoute(builder: (_) => const SettingsScreen())),
          ),
        ],
      ),
    );
  }

  // ── AI Detection Card ───────────────────────────────────────────────────────
  Widget _buildAiCard(reading) {
    final hasObj  = reading.hasObjectDetected;
    final aiOn    = reading.aiActive;
    final color   = hasObj
        ? (reading.isCriticalObject ? AppTheme.riskHigh : AppTheme.riskMedium)
        : AppTheme.textSecondary;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: hasObj
            ? color.withOpacity(0.07)
            : AppTheme.bgCard,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
            color: hasObj ? color.withOpacity(0.3) : AppTheme.border),
      ),
      child: Row(
        children: [
          // AI model status dot
          Column(
            children: [
              Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: aiOn ? AppTheme.riskNone : AppTheme.textSecondary,
                  boxShadow: aiOn
                      ? [BoxShadow(
                          color: AppTheme.riskNone.withOpacity(0.5),
                          blurRadius: 6)]
                      : null,
                ),
              ).animate(onPlay: (c) => c.repeat())
               .fadeIn(duration: 900.ms).then().fadeOut(duration: 900.ms),
              const SizedBox(height: 4),
              Text(
                aiOn ? 'AI ON' : 'AI OFF',
                style: TextStyle(
                  color: aiOn ? AppTheme.riskNone : AppTheme.textSecondary,
                  fontSize: 8,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 0.8,
                ),
              ),
            ],
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'ON-DEVICE DETECTION',
                  style: TextStyle(
                    color: AppTheme.textSecondary,
                    fontSize: 9,
                    letterSpacing: 1.3,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 4),
                Row(
                  children: [
                    if (hasObj) ...[
                      Text(reading.objectEmoji,
                          style: const TextStyle(fontSize: 18)),
                      const SizedBox(width: 8),
                    ],
                    Text(
                      hasObj
                          ? reading.object.toUpperCase()
                          : (aiOn ? 'Scanning...' : 'Model not loaded'),
                      style: TextStyle(
                        color: hasObj ? color : AppTheme.textSecondary,
                        fontSize: 16,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
          if (hasObj)
            Column(
              crossAxisAlignment: CrossAxisAlignment.end,
              children: [
                Text(
                  reading.confidencePct,
                  style: TextStyle(
                    color: color,
                    fontSize: 18,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                Text(
                  'confidence',
                  style: TextStyle(
                    color: color.withOpacity(0.6),
                    fontSize: 9,
                    letterSpacing: 0.5,
                  ),
                ),
              ],
            ),
        ],
      ),
    ).animate(key: ValueKey(reading.object + reading.confidence.toString()))
     .fadeIn(duration: 300.ms);
  }

  Widget _buildLiveStats(SessionManager session, reading) {
    final events = session.activeSession?.events ?? [];
    final aiEvents = events.where((e) => e.object != 'unknown').length;

    return Row(
      children: [
        Expanded(child: StatTile(
          label: 'Alerts',
          value: '${events.length}',
          icon: Icons.notifications_outlined,
          valueColor: events.isNotEmpty ? AppTheme.riskMedium : null,
        )),
        const SizedBox(width: 10),
        Expanded(child: StatTile(
          label: 'AI Hits',
          value: '$aiEvents',
          icon: Icons.psychology_outlined,
          valueColor: aiEvents > 0 ? AppTheme.accent : null,
        )),
        const SizedBox(width: 10),
        Expanded(child: StatTile(
          label: 'IR Hit',
          value: reading.irTriggered ? 'YES' : 'No',
          icon: Icons.sensors_rounded,
          valueColor: reading.irTriggered ? AppTheme.riskHigh : null,
        )),
      ],
    ).animate().fadeIn(delay: 200.ms);
  }

  Widget _buildSessionButton(
      BuildContext context, SessionManager session, BleService ble) {
    final active = session.isActive;
    final color  = active ? AppTheme.riskHigh : AppTheme.riskNone;

    return GestureDetector(
      onTap: () async {
        if (!ble.isConnected && !active) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
                content: Text('Connect to cane first'),
                duration: Duration(seconds: 2)),
          );
          return;
        }
        if (active) {
          await session.endSession();
        } else {
          session.startSession();
        }
      },
      child: Container(
        width: double.infinity,
        padding: const EdgeInsets.symmetric(vertical: 18),
        decoration: BoxDecoration(
          color: color.withOpacity(0.1),
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: color, width: 1.5),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              active
                  ? Icons.stop_circle_outlined
                  : Icons.play_circle_outlined,
              color: color,
              size: 22,
            ),
            const SizedBox(width: 10),
            Text(
              active ? 'END SESSION' : 'START SESSION',
              style: TextStyle(
                  color: color,
                  fontSize: 15,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 1.2),
            ),
          ],
        ),
      ),
    ).animate().fadeIn(delay: 300.ms);
  }

  Widget _buildSecondaryActions(BuildContext context) {
    return Column(
      children: [
        _ActionCard(
          icon: Icons.history_rounded,
          title: 'Alert Log',
          subtitle: 'Browse all obstacle events with AI labels',
          color: AppTheme.accent,
          onTap: () => Navigator.push(context,
              MaterialPageRoute(builder: (_) => const LogScreen())),
        ),
        const SizedBox(height: 10),
        _ActionCard(
          icon: Icons.science_outlined,
          title: 'Research Dashboard',
          subtitle: 'AI accuracy, object distribution, CSV export',
          color: AppTheme.accentAlt,
          onTap: () => Navigator.push(context,
              MaterialPageRoute(builder: (_) => const ResearchScreen())),
        ),
      ],
    ).animate().fadeIn(delay: 400.ms);
  }
}

class _ActionCard extends StatelessWidget {
  final IconData icon;
  final String title, subtitle;
  final Color color;
  final VoidCallback onTap;
  const _ActionCard(
      {required this.icon,
      required this.title,
      required this.subtitle,
      required this.color,
      required this.onTap});

  @override
  Widget build(BuildContext context) => GestureDetector(
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
              color: AppTheme.bgCard,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: AppTheme.border)),
          child: Row(children: [
            Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                    color: color.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(10)),
                child: Icon(icon, color: color, size: 20)),
            const SizedBox(width: 14),
            Expanded(
                child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                  Text(title,
                      style: const TextStyle(
                          fontWeight: FontWeight.w700,
                          fontSize: 14,
                          color: AppTheme.textPrimary)),
                  Text(subtitle,
                      style: const TextStyle(
                          fontSize: 11, color: AppTheme.textSecondary)),
                ])),
            Icon(Icons.chevron_right,
                color: AppTheme.textSecondary.withOpacity(0.4)),
          ]),
        ),
      );
}
