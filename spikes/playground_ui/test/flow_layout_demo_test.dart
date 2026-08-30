// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_playground_ui_spike/src/flow_layout_demo.dart';
import 'package:facetwire_playground_ui_spike/src/spike_app.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('loads the three-level uncompressed Flow scene package', (
    tester,
  ) async {
    final package = await FlowSceneLoader().load();

    expect(package.levels, hasLength(3));
    expect(package.levels[0].flowId, 'flow.level-1');
    expect(package.levels[1].flowId, 'flow.level-2');
    expect(package.levels[2].flowId, 'flow.level-3');
    expect(package.levels.every((level) => level.items.length == 3), isTrue);
    expect(
      package.levels[2].items['object.missing.level-3']!.contentType,
      'unknown',
    );
    expect(package.levels[0].canvasSize, const Size(720, 900));
    expect(
      package.levels[0].childBounds,
      const Rect.fromLTWH(430, 610, 250, 250),
    );
  });

  test('parses the native Flow layout report contract', () async {
    final source = await DemoRuntimeClient().composeFlowDemo(
      width: 600,
      height: 700,
      contentCase: 0,
      pageMode: 0,
    );

    final report = FlowPlanReport.fromJson(source);

    expect(report.capability, 'facetwire.layout.flow');
    expect(report.composeStatus, 0);
    expect(report.complete, isTrue);
    expect(report.fragmentCount, 3);
    expect(report.columnCount, 1);
    expect(report.fragments.map((fragment) => fragment.kind), [
      'text',
      'object',
      'text',
    ]);
  });

  testWidgets(
    'verifies levels, fallback, unsupported boundary, and viewer mode',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      await tester.pumpWidget(
        MaterialApp(
          home: FlowLayoutDemoScreen(
            client: DemoRuntimeClient(),
            loader: _MemoryFlowScenePackageLoader(),
          ),
        ),
      );
      await tester.pump();

      expect(find.text('Flow Layout 0.1 验证'), findsOneWidget);
      expect(find.byKey(const ValueKey('flow-level-0')), findsOneWidget);
      expect(find.byKey(const ValueKey('flow-level-1')), findsOneWidget);
      expect(find.byKey(const ValueKey('flow-level-2')), findsOneWidget);
      expect(find.text('3 fragments'), findsOneWidget);
      expect(
        find.byKey(const ValueKey('flow-recursive-canvas')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-level-surface-0')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-level-surface-1')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-level-surface-2')),
        findsOneWidget,
      );
      final fallbackWarning = find.byKey(
        const ValueKey('flow-native-fallback-warning'),
      );
      await tester.scrollUntilVisible(
        fallbackWarning,
        180,
        scrollable: find
            .descendant(
              of: find.byKey(const ValueKey('flow-layout-controls')),
              matching: find.byType(Scrollable),
            )
            .first,
      );
      expect(fallbackWarning, findsOneWidget);
      expect(
        find.byKey(
          const ValueKey('flow-fragment:paragraph.intro.level-1:page-0'),
        ),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-fragment:image.hero.level-1:page-0')),
        findsOneWidget,
      );

      final controlsScrollable = find
          .descendant(
            of: find.byKey(const ValueKey('flow-layout-controls')),
            matching: find.byType(Scrollable),
          )
          .first;
      final levelThree = find.byKey(const ValueKey('flow-level-2'));
      await tester.scrollUntilVisible(
        levelThree,
        -180,
        scrollable: controlsScrollable,
      );
      await tester.tap(levelThree);
      await tester.pump(const Duration(seconds: 1));
      expect(find.text('Placeholder / 后备占位'), findsOneWidget);
      expect(
        find.byKey(
          const ValueKey('flow-fragment:object.missing.level-3:page-0'),
        ),
        findsOneWidget,
      );

      final pageMode = find.byKey(const ValueKey('flow-page-mode'));
      await tester.scrollUntilVisible(
        pageMode,
        180,
        scrollable: controlsScrollable,
      );
      await tester.tap(find.text('虚拟页'));
      await tester.pump(const Duration(seconds: 1));
      expect(
        find.byKey(const ValueKey('flow-recursive-page-2-0')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-recursive-page-2-1')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('flow-recursive-page-0-2')),
        findsOneWidget,
      );
      expect(
        find.byKey(
          const ValueKey('flow-fragment:paragraph.intro.level-3:page-0'),
        ),
        findsOneWidget,
      );
      expect(
        find.byKey(
          const ValueKey('flow-fragment:object.missing.level-3:page-0'),
        ),
        findsOneWidget,
      );
      expect(
        find.byKey(
          const ValueKey('flow-fragment:paragraph.closing.level-3:page-1'),
        ),
        findsOneWidget,
      );

      await tester.tap(find.text('双栏'));
      await tester.pump(const Duration(seconds: 1));
      expect(find.byKey(const ValueKey('flow-column-0-0')), findsNWidgets(3));
      expect(find.byKey(const ValueKey('flow-column-0-1')), findsNWidgets(3));
      expect(
        find.byKey(
          const ValueKey('flow-fragment:paragraph.closing.level-3:page-0'),
        ),
        findsOneWidget,
      );
      expect(find.text('2 columns'), findsOneWidget);

      final layerOpacity = find.byKey(const ValueKey('flow-level-opacity-2'));
      await tester.scrollUntilVisible(
        layerOpacity,
        180,
        scrollable: controlsScrollable,
      );
      tester.widget<Slider>(layerOpacity).onChanged!(0.25);
      await tester.pump();
      expect(
        tester
            .widget<Opacity>(find.byKey(const ValueKey('flow-level-surface-2')))
            .opacity,
        0.25,
      );

      await tester.scrollUntilVisible(
        pageMode,
        -180,
        scrollable: controlsScrollable,
      );
      await tester.tap(find.text('连续'));
      await tester.pump(const Duration(seconds: 1));
      await tester.tap(find.text('固定 1:1'));
      await tester.pump();
      expect(find.byKey(const ValueKey('flow-actual-size')), findsOneWidget);

      final slider = find.byKey(const ValueKey('flow-preview-opacity'));
      await tester.drag(slider, const Offset(-180, 0));
      await tester.pump();
      expect(find.textContaining('预览不透明度'), findsOneWidget);
    },
  );
}

final class _MemoryFlowScenePackageLoader implements FlowScenePackageLoader {
  @override
  Future<FlowScenePackage> load([
    String descriptor = flowLayoutDemoDescriptor,
  ]) async {
    return FlowScenePackage(
      List.generate(3, (index) {
        final level = index + 1;
        final prefix = 'level-$level';
        final objectId = level == 3
            ? 'object.missing.$prefix'
            : 'image.hero.$prefix';
        return FlowSceneLevel(
          documentId: 'document:$prefix',
          title: 'Flow Level $level',
          flowId: 'flow.$prefix',
          width: 600,
          height: 700,
          canvasSize: const Size(600, 700),
          flowBounds: const Rect.fromLTWH(0, 0, 600, 700),
          childBounds: index < 2 ? const Rect.fromLTWH(50, 50, 300, 300) : null,
          childClip: index < 2,
          items: {
            'paragraph.intro.$prefix': FlowSceneItem(
              id: 'paragraph.intro.$prefix',
              kind: 'paragraph',
              text: 'Level $level intro',
              contentType: '',
              resourceAsset: null,
            ),
            objectId: FlowSceneItem(
              id: objectId,
              kind: 'object',
              text: '',
              contentType: level == 3 ? 'unknown' : 'image',
              resourceAsset: null,
            ),
            'paragraph.closing.$prefix': FlowSceneItem(
              id: 'paragraph.closing.$prefix',
              kind: 'paragraph',
              text: 'Level $level closing',
              contentType: '',
              resourceAsset: null,
            ),
          },
        );
      }),
    );
  }
}
