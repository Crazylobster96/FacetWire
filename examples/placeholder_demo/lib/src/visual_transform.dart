// SPDX-License-Identifier: MPL-2.0
import 'package:flutter/widgets.dart';

/// Playground projection of FacetWire VisualTransform 0.1.
///
/// Native renderers remain authoritative. This value object keeps Flutter host
/// previews from inventing different placement and rotation defaults.
final class VisualTransformSpec {
  const VisualTransformSpec({
    this.fit = BoxFit.contain,
    this.alignment = Alignment.center,
    this.clip = true,
    this.contentRotationQuarterTurns = 0,
  }) : assert(contentRotationQuarterTurns >= 0),
       assert(contentRotationQuarterTurns <= 3);

  factory VisualTransformSpec.fromPlacement(Map<String, Object?>? placement) {
    final value = placement ?? const <String, Object?>{};
    final alignmentValue =
        value['alignment'] as Map<String, Object?>? ?? const {};
    final quarterTurns =
        (value['contentRotationQuarterTurns'] as num?)?.toInt() ?? 0;
    if (quarterTurns < 0 || quarterTurns > 3) {
      throw FormatException(
        'contentRotationQuarterTurns must be an integer from 0 through 3',
      );
    }
    return VisualTransformSpec(
      fit: switch (value['fit'] as String? ?? 'contain') {
        'none' => BoxFit.none,
        'cover' => BoxFit.cover,
        'fill' => BoxFit.fill,
        'contain' => BoxFit.contain,
        final invalid => throw FormatException(
          'Unsupported visual fit: $invalid',
        ),
      },
      alignment: Alignment(
        (((alignmentValue['x'] as num?) ?? 0.5).toDouble() * 2) - 1,
        (((alignmentValue['y'] as num?) ?? 0.5).toDouble() * 2) - 1,
      ),
      clip: value['clip'] as bool? ?? true,
      contentRotationQuarterTurns: quarterTurns,
    );
  }

  final BoxFit fit;
  final Alignment alignment;
  final bool clip;
  final int contentRotationQuarterTurns;

  Widget applyToVisual(Widget child, {Key? key}) {
    Widget result = RotatedBox(
      key: key,
      quarterTurns: contentRotationQuarterTurns,
      child: child,
    );
    if (clip) result = ClipRect(child: result);
    return result;
  }
}
