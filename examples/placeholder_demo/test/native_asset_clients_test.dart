// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/flow_runtime_client.dart';
import 'package:facetwire_placeholder_demo/src/chart_runtime_client.dart';
import 'package:facetwire_placeholder_demo/src/native_asset_demo_client.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test(
    'one Native Asset serves Placeholder, Flow, and Chart contracts',
    () async {
      final placeholder = NativeAssetDemoClient.open();
      final flow = NativeAssetRuntimeClient.open();
      final chart = NativeAssetChartRuntimeClient.open();
      try {
        final snapshot = await placeholder.snapshot();
        final report = jsonDecode(
          await flow.composeFlowDemo(
            width: 600,
            height: 700,
            contentCase: 0,
            pageMode: 0,
          ),
        ) as Map<String, Object?>;
        final columns = jsonDecode(
          await flow.composeFlowDemo(
            width: 600,
            height: 700,
            contentCase: 2,
            pageMode: 2,
          ),
        ) as Map<String, Object?>;
        final inline = jsonDecode(
          await flow.composeFlowDemo(
            width: 600,
            height: 700,
            contentCase: 3,
            pageMode: 0,
          ),
        ) as Map<String, Object?>;
        final floatStart = jsonDecode(
          await flow.composeFlowDemo(
            width: 600,
            height: 700,
            contentCase: 6,
            pageMode: 0,
          ),
        ) as Map<String, Object?>;
        final bar = await chart.render(
          width: 640,
          height: 360,
          kind: ChartDemoKind.bar,
          rotation: 1,
          opacity: 0.72,
        );

        expect(
          snapshot['pluginId'],
          'org.facetwire.reference.placeholder-renderer',
        );
        expect(report['capability'], 'facetwire.layout.flow');
        expect(report['nativeRuntime'], isTrue);
        expect(report['fragmentCount'], 3);
        expect(columns['nativeRuntime'], isTrue);
        expect(columns['pageMode'], 2);
        expect(columns['pageCount'], 1);
        expect(columns['columnCount'], 2);
        final columnFragments = columns['fragments']! as List<Object?>;
        expect(
          (columnFragments.last! as Map<String, Object?>)['columnIndex'],
          1,
        );
        expect(inline['nativeRuntime'], isTrue);
        expect(inline['inlineObjects'], isTrue);
        expect(inline['fragmentCount'], 3);
        final inlineFragments = inline['fragments']! as List<Object?>;
        expect(
          (inlineFragments[1]! as Map<String, Object?>)['sourceItemId'],
          'image.inline.level-1',
        );
        expect((inlineFragments.last! as Map<String, Object?>)['textStart'], 7);
        expect(floatStart['placementMode'], 'float-start');
        expect(floatStart['inlineObjects'], isFalse);
        expect(floatStart['fragmentCount'], 3);
        expect(bar.nativeRuntime, isTrue);
        expect(bar.pluginId, 'org.facetwire.reference.core-chart-renderer');
        expect(bar.kind, 'bar');
        expect(bar.transform.rotation, 1);
        expect(bar.renderedSeries, 2);
        expect(bar.renderedValues, 8);
        expect(bar.commandsBalanced, isTrue);
        expect(bar.uncoveredIsTransparent, isTrue);
        expect(bar.elements, isNotEmpty);
        expect(
          bar.commands.every((command) => command.elementId.isNotEmpty),
          isTrue,
        );
        expect(
          bar.commands.where(
            (command) =>
                command.type == 'rect' && command.categoryId.isNotEmpty,
          ),
          hasLength(8),
        );
        expect(
          bar.commands.where(
            (command) => command.elementId.contains('/legend-marker/'),
          ),
          hasLength(2),
        );
        for (final kind in ChartDemoKind.values) {
          final advanced = await chart.render(
            width: 640,
            height: 360,
            kind: kind,
            rotation: kind.index % 4,
            opacity: 0.72,
          );
          expect(advanced.nativeRuntime, isTrue, reason: kind.name);
          expect(advanced.kind, isNotEmpty, reason: kind.name);
          expect(advanced.commandsBalanced, isTrue, reason: kind.name);
          expect(advanced.commands, isNotEmpty, reason: kind.name);
          expect(advanced.uncoveredIsTransparent, isTrue, reason: kind.name);
        }
        final revenueQ2 = bar.elements.singleWhere(
          (element) => element.id.endsWith('/datum/revenue/q2'),
        );
        final adjusted = await chart.render(
          width: 640,
          height: 360,
          kind: ChartDemoKind.bar,
          rotation: 0,
          opacity: 0.9,
          adjustment: ChartElementAdjustment(
            elementIndex: revenueQ2.index,
            opacity: 0.35,
            translateX: 0.05,
            translateY: -0.03,
            scale: 1.12,
            rotationRadians: 0.25,
            promoted: true,
            accentColor: true,
          ),
        );
        expect(adjusted.selectedElementIndex, revenueQ2.index);
        final adjustedCommands = adjusted.commands.where(
          (command) => command.elementId == revenueQ2.id,
        );
        expect(adjustedCommands, isNotEmpty);
        expect(adjustedCommands.every((command) => command.promoted), isTrue);
        expect(
          adjustedCommands.every((command) => command.zIndex == 150),
          isTrue,
        );
        expect(adjustedCommands.first.color[0], closeTo(0.94, 0.001));
        expect(adjustedCommands.first.color[1], closeTo(0.45, 0.001));
        expect(adjustedCommands.first.color[2], closeTo(0.16, 0.001));
        expect(adjustedCommands.first.color[3], closeTo(0.35, 0.001));
      } finally {
        await placeholder.close();
        await flow.close();
        await chart.close();
      }
    },
  );
}
