// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_playground_ui_spike/src/spike_app.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('shows runtime state, opacity control, canvas, and semantics',
      (tester) async {
    await tester.pumpWidget(SpikeApp(client: DemoRuntimeClient()));
    await tester.pumpAndSettle();

    expect(find.textContaining('"renderer":"demo"'), findsOneWidget);
    expect(find.byKey(const ValueKey('opacity-slider')), findsOneWidget);
    expect(find.byKey(const ValueKey('display-list-canvas')), findsOneWidget);
    expect(
      find.bySemanticsLabel('Unsupported FacetWire zone placeholder'),
      findsOneWidget,
    );
  });
}
