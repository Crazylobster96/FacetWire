// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

enum PlaceholderReason {
  loading(1, 'loading'),
  rendererMissing(2, 'renderer_missing'),
  unsupportedType(3, 'unsupported_type'),
  resourceMissing(4, 'resource_missing'),
  resourceUnavailable(5, 'resource_unavailable'),
  parseFailed(6, 'parse_failed'),
  decodeFailed(7, 'decode_failed'),
  policyBlocked(8, 'policy_blocked'),
  permissionRequired(9, 'permission_required'),
  resourceLimited(10, 'resource_limited'),
  pluginFailed(11, 'plugin_failed'),
  unknown(12, 'unknown');

  const PlaceholderReason(this.wireValue, this.documentValue);
  final int wireValue;
  final String documentValue;

  static PlaceholderReason fromDocument(String value) => values.firstWhere(
    (candidate) => candidate.documentValue == value,
    orElse: () => unknown,
  );
}

enum PlaceholderMode {
  hidden(1, 'hidden'),
  minimal(2, 'minimal'),
  standard(3, 'standard'),
  diagnostic(4, 'diagnostic');

  const PlaceholderMode(this.wireValue, this.documentValue);
  final int wireValue;
  final String documentValue;

  static PlaceholderMode fromDocument(String value) => values.firstWhere(
    (candidate) => candidate.documentValue == value,
    orElse: () => standard,
  );
}

enum PlaceholderPhase {
  none(0, 'none'),
  queued(1, 'queued'),
  running(2, 'running'),
  waiting(3, 'waiting'),
  transferring(4, 'transferring'),
  readyForHandoff(5, 'ready_for_handoff');

  const PlaceholderPhase(this.wireValue, this.label);
  final int wireValue;
  final String label;
}

enum MeasureCase {
  kindFallback(0, 'Kind fallback'),
  resolved(1, 'Resolved size'),
  intrinsic(2, 'Intrinsic size'),
  widthAndRatio(3, 'Width + ratio'),
  explicitConstraints(4, 'Explicit constraints');

  const MeasureCase(this.wireValue, this.label);
  final int wireValue;
  final String label;
}

abstract final class PlaceholderAction {
  static const int retry = 1 << 0;
  static const int showDetails = 1 << 1;
  static const int locate = 1 << 2;
  static const int permission = 1 << 3;
  static const int findPlugin = 1 << 4;
  static const int alternative = 1 << 5;
  static const int all = (1 << 6) - 1;

  static int fromNames(Iterable<String> names) {
    var result = 0;
    for (final name in names) {
      result |= switch (name) {
        'retry' => retry,
        'show_details' => showDetails,
        'locate' => locate,
        'permission' => permission,
        'find_plugin' => findPlugin,
        'alternative' => alternative,
        _ => 0,
      };
    }
    return result;
  }

  static String label(int action) => switch (action) {
    retry => 'Retry',
    showDetails => 'Show details',
    locate => 'Locate resource',
    permission => 'Request permission',
    findPlugin => 'Find plugin',
    alternative => 'Use alternative',
    _ => 'No action',
  };
}

final class DemoNativeRequest {
  const DemoNativeRequest({
    required this.width,
    required this.height,
    required this.opacity,
    required this.backgroundAlpha,
    required this.fontScale,
    required this.devicePixelRatio,
    required this.reason,
    required this.mode,
    required this.permittedActions,
    required this.phase,
    required this.progressKind,
    required this.completed,
    required this.total,
    required this.stale,
    required this.prefersDark,
    required this.highContrast,
    required this.reduceMotion,
    required this.measureCase,
    required this.presentationRevision,
    required this.contentKind,
    required this.label,
  });

  final double width;
  final double height;
  final double opacity;
  final double backgroundAlpha;
  final double fontScale;
  final double devicePixelRatio;
  final PlaceholderReason reason;
  final PlaceholderMode mode;
  final int permittedActions;
  final PlaceholderPhase phase;
  final int progressKind;
  final int completed;
  final int total;
  final bool stale;
  final bool prefersDark;
  final bool highContrast;
  final bool reduceMotion;
  final MeasureCase measureCase;
  final int presentationRevision;
  final String contentKind;
  final String label;
}

final class DemoCommand {
  const DemoCommand({
    required this.op,
    required this.x,
    required this.y,
    required this.width,
    required this.height,
    required this.radius,
    required this.strokeWidth,
    required this.dashed,
    required this.red,
    required this.green,
    required this.blue,
    required this.alpha,
    required this.value,
  });

  factory DemoCommand.fromJson(Map<String, Object?> json) => DemoCommand(
    op: json['op']! as String,
    x: (json['x']! as num).toDouble(),
    y: (json['y']! as num).toDouble(),
    width: (json['width']! as num).toDouble(),
    height: (json['height']! as num).toDouble(),
    radius: (json['radius']! as num).toDouble(),
    strokeWidth: (json['strokeWidth']! as num).toDouble(),
    dashed: (json['dashed']! as num).toInt() != 0,
    red: (json['red']! as num).toDouble(),
    green: (json['green']! as num).toDouble(),
    blue: (json['blue']! as num).toDouble(),
    alpha: (json['alpha']! as num).toDouble(),
    value: json['value']! as String,
  );

  final String op;
  final double x;
  final double y;
  final double width;
  final double height;
  final double radius;
  final double strokeWidth;
  final bool dashed;
  final double red;
  final double green;
  final double blue;
  final double alpha;
  final String value;
}

final class DemoRenderReport {
  const DemoRenderReport({
    required this.contract,
    required this.measure,
    required this.render,
    required this.semantics,
    required this.commands,
  });

  factory DemoRenderReport.fromJsonString(String source) {
    final json = jsonDecode(source) as Map<String, Object?>;
    return DemoRenderReport(
      contract: json['contract']! as Map<String, Object?>,
      measure: json['measure']! as Map<String, Object?>,
      render: json['render']! as Map<String, Object?>,
      semantics: json['semantics']! as Map<String, Object?>,
      commands: (json['commands']! as List<Object?>)
          .cast<Map<String, Object?>>()
          .map(DemoCommand.fromJson)
          .toList(growable: false),
    );
  }

  final Map<String, Object?> contract;
  final Map<String, Object?> measure;
  final Map<String, Object?> render;
  final Map<String, Object?> semantics;
  final List<DemoCommand> commands;
}

abstract interface class NativeDemoClient {
  Future<Map<String, Object?>> snapshot();
  Future<Map<String, Object?>> parameterSchema();
  Future<DemoRenderReport> render(DemoNativeRequest request);
  Future<int> hitTest(DemoNativeRequest request, double x, double y);
  Future<void> close();
}
