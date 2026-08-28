// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_placeholder_demo/src/visual_transform.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('parses shared image gif video chart placement defaults', () {
    final transform = VisualTransformSpec.fromPlacement({
      'fit': 'cover',
      'alignment': {'x': 0.25, 'y': 0.75},
      'clip': false,
      'contentRotationQuarterTurns': 3,
    });

    expect(transform.fit, BoxFit.cover);
    expect(transform.alignment, const Alignment(-0.5, 0.5));
    expect(transform.clip, isFalse);
    expect(transform.contentRotationQuarterTurns, 3);
  });

  testWidgets('applies rotation without adding a background', (tester) async {
    const transform = VisualTransformSpec(contentRotationQuarterTurns: 1);
    await tester.pumpWidget(
      transform.applyToVisual(const SizedBox(width: 160, height: 90)),
    );

    final rotated = tester.widget<RotatedBox>(find.byType(RotatedBox));
    expect(rotated.quarterTurns, 1);
    expect(find.byType(ColoredBox), findsNothing);
  });

  test('rejects unsupported rotation values', () {
    expect(
      () =>
          VisualTransformSpec.fromPlacement({'contentRotationQuarterTurns': 4}),
      throwsFormatException,
    );
  });
}
