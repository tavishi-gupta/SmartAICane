import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import '../theme/app_theme.dart';
import '../models/models.dart';

// ── Risk Display ──────────────────────────────────────────────────────────────
class RiskDisplay extends StatelessWidget {
  final CaneReading reading;
  const RiskDisplay({super.key, required this.reading});

  @override
  Widget build(BuildContext context) {
    final color = AppTheme.forRisk(reading.risk);
    final label = AppTheme.labelForRisk(reading.risk);

    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: color.withOpacity(0.08),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withOpacity(0.35), width: 1.5),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(AppTheme.iconForRisk(reading.risk), color: color, size: 36)
              .animate(key: ValueKey(reading.risk))
              .scale(duration: 300.ms, curve: Curves.easeOut),
          const SizedBox(height: 10),
          Text(
            reading.distanceCm < 990 ? '${reading.distanceCm} cm' : '—',
            style: TextStyle(
              color: color,
              fontSize: 52,
              fontWeight: FontWeight.w700,
              height: 1.0,
            ),
          ),
          const SizedBox(height: 6),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 4),
            decoration: BoxDecoration(
              color: color.withOpacity(0.15),
              borderRadius: BorderRadius.circular(99),
            ),
            child: Text(
              label,
              style: TextStyle(
                color: color,
                fontSize: 13,
                fontWeight: FontWeight.w700,
                letterSpacing: 1.5,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// ── Connection Pill ───────────────────────────────────────────────────────────
class ConnectionPill extends StatelessWidget {
  final bool connected;
  final String label;
  final VoidCallback? onTap;
  const ConnectionPill(
      {super.key,
      required this.connected,
      required this.label,
      this.onTap});

  @override
  Widget build(BuildContext context) {
    final color = connected ? AppTheme.riskNone : AppTheme.textSecondary;
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        decoration: BoxDecoration(
          color: color.withOpacity(0.1),
          borderRadius: BorderRadius.circular(99),
          border: Border.all(color: color.withOpacity(0.3)),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 7,
              height: 7,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                boxShadow: connected
                    ? [BoxShadow(color: color.withOpacity(0.5), blurRadius: 6)]
                    : null,
              ),
            )
                .animate(onPlay: (c) => c.repeat())
                .fadeIn(duration: 800.ms)
                .then()
                .fadeOut(duration: 800.ms),
            const SizedBox(width: 7),
            Text(label,
                style: TextStyle(
                    color: color,
                    fontSize: 12,
                    fontWeight: FontWeight.w600)),
          ],
        ),
      ),
    );
  }
}

// ── Stat Tile ─────────────────────────────────────────────────────────────────
class StatTile extends StatelessWidget {
  final String label, value;
  final IconData? icon;
  final Color? valueColor;
  const StatTile(
      {super.key,
      required this.label,
      required this.value,
      this.icon,
      this.valueColor});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: AppTheme.bgCard,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppTheme.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(children: [
            if (icon != null) ...[
              Icon(icon, size: 11, color: AppTheme.textSecondary),
              const SizedBox(width: 4),
            ],
            Text(label.toUpperCase(),
                style: const TextStyle(
                    color: AppTheme.textSecondary,
                    fontSize: 9,
                    letterSpacing: 1.1,
                    fontWeight: FontWeight.w600)),
          ]),
          const SizedBox(height: 5),
          Text(value,
              style: TextStyle(
                  color: valueColor ?? AppTheme.textPrimary,
                  fontSize: 18,
                  fontWeight: FontWeight.w700)),
        ],
      ),
    );
  }
}

// ── Section Header ────────────────────────────────────────────────────────────
class SectionHeader extends StatelessWidget {
  final String title;
  final Widget? trailing;
  const SectionHeader({super.key, required this.title, this.trailing});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 0, vertical: 6),
      child: Row(
        children: [
          Container(
              width: 3,
              height: 14,
              color: AppTheme.accent,
              margin: const EdgeInsets.only(right: 8)),
          Text(title.toUpperCase(),
              style: const TextStyle(
                  color: AppTheme.textSecondary,
                  fontSize: 10,
                  letterSpacing: 1.5,
                  fontWeight: FontWeight.w600)),
          const Spacer(),
          if (trailing != null) trailing!,
        ],
      ),
    );
  }
}

// ── Alert Event Row (v2) ──────────────────────────────────────────────────────
// Now shows object emoji, confidence bar, and AI badge
class AlertEventRow extends StatelessWidget {
  final AlertEvent event;
  final VoidCallback? onFlagFP;
  const AlertEventRow({super.key, required this.event, this.onFlagFP});

  @override
  Widget build(BuildContext context) {
    final color      = AppTheme.forRisk(event.risk);
    final hasObj     = event.object != 'unknown';
    final confFrac   = event.confidence / 255.0;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppTheme.bgCard,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppTheme.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              // Risk dot
              Container(
                width: 8,
                height: 8,
                margin: const EdgeInsets.only(top: 1),
                decoration: BoxDecoration(shape: BoxShape.circle, color: color),
              ),
              const SizedBox(width: 8),
              // Distance + risk
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(children: [
                      Text(
                        '${event.distanceCm} cm — ${event.risk}',
                        style: TextStyle(
                            color: color,
                            fontWeight: FontWeight.w700,
                            fontSize: 13),
                      ),
                      if (event.aiActive) ...[
                        const SizedBox(width: 6),
                        Container(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 5, vertical: 1),
                          decoration: BoxDecoration(
                            color: AppTheme.accent.withOpacity(0.15),
                            borderRadius: BorderRadius.circular(3),
                          ),
                          child: const Text('AI',
                              style: TextStyle(
                                  color: AppTheme.accent,
                                  fontSize: 8,
                                  fontWeight: FontWeight.w800)),
                        ),
                      ],
                    ]),
                    Text(
                      _fmtTime(event.time),
                      style: const TextStyle(
                          color: AppTheme.textSecondary, fontSize: 10),
                    ),
                  ],
                ),
              ),
              // Object emoji + label
              if (hasObj)
                Column(
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: [
                    Text(_objEmoji(event.object),
                        style: const TextStyle(fontSize: 16)),
                    Text(event.object,
                        style: TextStyle(
                            color: color.withOpacity(0.8),
                            fontSize: 10,
                            fontWeight: FontWeight.w600)),
                  ],
                ),
              // Flag / FP badge
              const SizedBox(width: 8),
              if (event.falsePositive == 1)
                Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                  decoration: BoxDecoration(
                    color: AppTheme.riskMedium.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: const Text('FP',
                      style: TextStyle(
                          color: AppTheme.riskMedium,
                          fontSize: 9,
                          fontWeight: FontWeight.w700)),
                )
              else if (onFlagFP != null)
                GestureDetector(
                  onTap: onFlagFP,
                  child: const Padding(
                    padding: EdgeInsets.all(4),
                    child: Icon(Icons.flag_outlined,
                        size: 14, color: AppTheme.textSecondary),
                  ),
                ),
            ],
          ),

          // Confidence bar (only if AI detected something)
          if (hasObj && event.confidence > 0) ...[
            const SizedBox(height: 8),
            Row(children: [
              const Text('confidence',
                  style: TextStyle(
                      color: AppTheme.textSecondary,
                      fontSize: 9,
                      letterSpacing: 0.5)),
              const SizedBox(width: 8),
              Expanded(
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(2),
                  child: LinearProgressIndicator(
                    value: confFrac,
                    backgroundColor: AppTheme.border,
                    valueColor: AlwaysStoppedAnimation(color),
                    minHeight: 4,
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Text(
                '${(confFrac * 100).round()}%',
                style: TextStyle(
                    color: color,
                    fontSize: 9,
                    fontWeight: FontWeight.w700),
              ),
            ]),
          ],
        ],
      ),
    );
  }

  String _objEmoji(String obj) {
    const m = {
      'person': '🚶', 'chair': '🪑', 'stairs': '🪜',
      'door': '🚪',   'car': '🚗',   'wall': '🧱', 'pole': '🪧',
    };
    return m[obj] ?? '❓';
  }

  String _fmtTime(DateTime t) =>
      '${t.hour.toString().padLeft(2, '0')}:'
      '${t.minute.toString().padLeft(2, '0')}:'
      '${t.second.toString().padLeft(2, '0')}';
}
