// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_placeholder_demo/src/chart_renderer_demo.dart';
import 'package:facetwire_placeholder_demo/src/chart_runtime_client.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets(
    'switches chart kinds and exposes opacity and transform controls',
    (tester) async {
      await tester.binding.setSurfaceSize(const Size(1100, 760));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      await tester.pumpWidget(
        const MaterialApp(
          home: ChartRendererDemoScreen(client: DemoChartRuntimeClient()),
        ),
      );
      await tester.pumpAndSettle();

      expect(
        find.byKey(const ValueKey('native-chart-preview')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('native-chart-viewport-clip')),
        findsOneWidget,
      );
      await tester.tap(find.byKey(const ValueKey('native-chart-preview')));
      await tester.pump();
      expect(find.byKey(const ValueKey('chart-glass-tooltip')), findsOneWidget);
      final advancedChoices = <ChartDemoKind>[
        ChartDemoKind.stackedBar,
        ChartDemoKind.area,
        ChartDemoKind.bubble,
        ChartDemoKind.donut,
        ChartDemoKind.radar,
        ChartDemoKind.heatmap,
        ChartDemoKind.gauge,
        ChartDemoKind.boxPlot,
        ChartDemoKind.waterfall,
        ChartDemoKind.funnel,
        ChartDemoKind.candlestick,
        ChartDemoKind.timeSeries,
        ChartDemoKind.combo,
        ChartDemoKind.divergingBar,
        ChartDemoKind.facetLine,
        ChartDemoKind.rangeArea,
        ChartDemoKind.densityHeatmap,
        ChartDemoKind.wordCloud,
        ChartDemoKind.rose,
        ChartDemoKind.treemap,
        ChartDemoKind.sunburst,
        ChartDemoKind.packedBubble,
      ];
      for (final kind in advancedChoices) {
        expect(find.byKey(ValueKey('chart-kind-${kind.name}')), findsOneWidget);
      }
      expect(find.byKey(const ValueKey('chart-theme')), findsOneWidget);
      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-legend-placement')),
        180,
        scrollable: find.byType(Scrollable).first,
      );
      expect(
        find.byKey(const ValueKey('chart-legend-placement')),
        findsOneWidget,
      );
      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-label-policy')),
        180,
        scrollable: find.byType(Scrollable).first,
      );
      expect(find.byKey(const ValueKey('chart-label-policy')), findsOneWidget);
      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-auto-layout')),
        180,
        scrollable: find.byType(Scrollable).first,
      );
      expect(find.byKey(const ValueKey('chart-auto-layout')), findsOneWidget);
      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-opacity')),
        260,
        scrollable: find.byType(Scrollable).first,
      );
      expect(find.byKey(const ValueKey('chart-opacity')), findsOneWidget);

      final elementSelector = find.byKey(
        const ValueKey('chart-element-selector'),
      );
      await tester.scrollUntilVisible(
        elementSelector,
        260,
        scrollable: find.byType(Scrollable).first,
      );
      await tester.tap(elementSelector);
      await tester.pumpAndSettle();
      await tester.tap(find.text('图表根 · Fallback chart').last);
      await tester.pumpAndSettle();
      expect(
        find.byKey(const ValueKey('chart-element-opacity')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('chart-element-translate-x')),
        findsOneWidget,
      );
      expect(find.byKey(const ValueKey('chart-element-scale')), findsOneWidget);
      expect(
        find.byKey(const ValueKey('chart-element-promoted')),
        findsOneWidget,
      );

      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-report-summary')),
        300,
        scrollable: find.byType(Scrollable).first,
      );
      expect(find.text('Dart fallback'), findsOneWidget);
      expect(find.text('Transparent PASS'), findsOneWidget);

      await tester.drag(find.byType(ListView), const Offset(0, 2000));
      await tester.pumpAndSettle();
      final lineChoice = find.byKey(const ValueKey('chart-kind-line'));
      expect(lineChoice, findsOneWidget);
      await tester.tap(lineChoice);
      await tester.pumpAndSettle();
      await tester.scrollUntilVisible(
        find.byKey(const ValueKey('chart-report-summary')),
        240,
        scrollable: find.byType(Scrollable).first,
      );
      expect(find.textContaining('line · 1 series'), findsOneWidget);

      final rotationControl = tester.widget<SegmentedButton<int>>(
        find.byType(SegmentedButton<int>),
      );
      rotationControl.onSelectionChanged?.call({1});
      await tester.pumpAndSettle();
    },
  );
}
