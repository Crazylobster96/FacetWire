// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_playground_ui_spike/src/spike_app.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('shows runtime state, opacity control, canvas, and semantics', (
    tester,
  ) async {
    final semantics = tester.ensureSemantics();

    await tester.pumpWidget(SpikeApp(client: DemoRuntimeClient()));
    await tester.pump();
    await tester.tap(find.byKey(const ValueKey('open-placeholder-demo')));
    await tester.pumpAndSettle();

    expect(find.textContaining('"renderer":"demo"'), findsOneWidget);
    final opacitySlider = find.byKey(const ValueKey('opacity-slider'));
    expect(opacitySlider, findsOneWidget);
    final opacitySemantics = find.semantics.byLabel('不透明度');
    expect(opacitySemantics, findsOne);
    expect(
      tester.getSemantics(opacitySlider),
      isSemantics(
        label: '不透明度',
        value: '75%',
        increasedValue: '80%',
        decreasedValue: '70%',
        isSlider: true,
        isEnabled: true,
        isFocusable: true,
        hasIncreaseAction: true,
        hasDecreaseAction: true,
      ),
    );
    tester.semantics.increase(opacitySemantics);
    await tester.pump();
    expect(opacitySemantics, isSemantics(label: '不透明度', value: '80%'));
    expect(find.text('80%'), findsOneWidget);
    expect(find.byKey(const ValueKey('display-list-canvas')), findsOneWidget);
    expect(
      find.bySemanticsLabel('Unsupported FacetWire zone placeholder'),
      findsOneWidget,
    );
    semantics.dispose();
  });
}
