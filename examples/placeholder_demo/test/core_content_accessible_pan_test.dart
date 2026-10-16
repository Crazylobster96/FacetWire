// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

const _descriptor = 'synthetic/pan.dis.json';
const _viewportKey = ValueKey('canvas-accessible-viewport');
const _viewerKey = ValueKey('canvas-interactive-viewer');
const _directions = <SemanticsAction>[
  SemanticsAction.scrollUp,
  SemanticsAction.scrollDown,
  SemanticsAction.scrollLeft,
  SemanticsAction.scrollRight,
];

class _PanBundle extends CachingAssetBundle {
  _PanBundle(this.size);
  final Size size;

  @override
  Future<ByteData> load(String key) async {
    if (key != _descriptor) throw FlutterError('Unknown synthetic asset: $key');
    final descriptor = {
      'format': 'facetwire.agent-scene-package',
      'version': '0.1',
      'id': 'accessible-pan',
      'title': 'Synthetic pan',
      'resources': <Object>[],
      'canvas': {
        'size': {'width': size.width, 'height': size.height},
        'pages': [
          {
            'layers': [
              {
                'id': 'layer',
                'z': 0,
                'zones': [
                  for (final entry in [
                    ('start', 10.0),
                    ('end', size.height - 40),
                  ])
                    {
                      'id': entry.$1,
                      'bounds': {
                        'x': 10,
                        'y': entry.$2,
                        'width': size.width - 20,
                        'height': 30,
                      },
                      'content': {
                        'type': 'text',
                        'text': 'Synthetic ${entry.$1}',
                        'selectable': true,
                      },
                    },
                ],
              },
            ],
          },
        ],
      },
    };
    return ByteData.sublistView(
      Uint8List.fromList(utf8.encode(jsonEncode(descriptor))),
    );
  }
}

Future<void> _load(WidgetTester tester, Size canvas) async {
  await tester.binding.setSurfaceSize(const Size(390, 844));
  await tester.pumpWidget(
    MaterialApp(
      home: CoreContentDemoScreen(
        loader: CoreContentPackageLoader(bundle: _PanBundle(canvas)),
        descriptorAsset: _descriptor,
        showDiagnosticBadges: false,
      ),
    ),
  );
  await tester.pumpAndSettle();
}

Future<void> _mode(WidgetTester tester, DemoCanvasScaleMode mode) async {
  final control = tester.widget<SegmentedButton<DemoCanvasScaleMode>>(
    find.byKey(const ValueKey('canvas-scale-mode')),
  );
  control.onSelectionChanged!({mode});
  await tester.pumpAndSettle();
}

SemanticsNode _node(WidgetTester tester) =>
    tester.getSemantics(find.byKey(_viewportKey));
TransformationController _controller(WidgetTester tester) => tester
    .widget<InteractiveViewer>(find.byKey(_viewerKey))
    .transformationController!;

Future<void> _action(WidgetTester tester, SemanticsAction action) async {
  final node = _node(tester);
  expect(node.getSemanticsData().hasAction(action), isTrue);
  node.owner!.performAction(node.id, action);
  await tester.pumpAndSettle();
}

void main() {
  final platforms = TargetPlatformVariant({
    TargetPlatform.iOS,
    TargetPlatform.macOS,
  });
  testWidgets(
    'inspector ListView scroll is independent from the content canvas',
    (tester) async {
      final semantics = tester.ensureSemantics();
      addTearDown(() => tester.binding.setSurfaceSize(null));
      try {
        await _load(tester, const Size(640, 1200));
        final controls = find.byKey(const ValueKey('core-content-controls'));
        final state = tester.state<ScrollableState>(
          find
              .descendant(of: controls, matching: find.byType(Scrollable))
              .first,
        );
        expect(state.position.maxScrollExtent, greaterThan(0));
        final originalPosition = state.position.pixels;
        final originalCanvas = _controller(tester).value.clone();
        final candidates = <SemanticsNode>[];
        void inspect(SemanticsNode node) {
          final data = node.getSemanticsData();
          if (data.flagsCollection.hasImplicitScrolling &&
              data.hasAction(SemanticsAction.scrollUp)) {
            candidates.add(node);
          }
          node.visitChildren((child) {
            inspect(child);
            return true;
          });
        }

        inspect(
          tester
              .binding
              .renderViews
              .single
              .owner!
              .semanticsOwner!
              .rootSemanticsNode!,
        );
        expect(
          candidates,
          hasLength(1),
          reason: 'The actual control list must expose its own native scroll action',
        );
        final listNode = candidates.single;
        // Flutter directions name finger movement: swipe up reveals later rows.
        listNode.owner!.performAction(listNode.id, SemanticsAction.scrollUp);
        await tester.pumpAndSettle();
        expect(state.position.pixels, greaterThan(originalPosition));
        expect(
          _controller(tester).value,
          originalCanvas,
          reason: 'Scrolling controls must not pan the document',
        );
        await _mode(tester, DemoCanvasScaleMode.actualSize);
        final controlPosition = state.position.pixels;
        await _action(tester, SemanticsAction.scrollUp);
        expect(
          state.position.pixels,
          controlPosition,
          reason: 'Panning the document must not scroll the control list',
        );
      } finally {
        await tester.pumpWidget(const SizedBox.shrink());
        semantics.dispose();
      }
    },
    variant: platforms,
  );

  testWidgets(
    'actual canvas exposes directional scroll and keeps descendants',
    (tester) async {
      final semantics = tester.ensureSemantics();
      addTearDown(() => tester.binding.setSurfaceSize(null));
      try {
        await _load(tester, const Size(640, 1200));
        await _mode(tester, DemoCanvasScaleMode.actualSize);
        expect(
          find.byKey(const ValueKey('document-canvas:accessible-pan')),
          findsOneWidget,
        );
        expect(find.byKey(_viewerKey), findsOneWidget);
        var directionalContainers = 0;
        void inspect(SemanticsNode candidate) {
          final data = candidate.getSemanticsData();
          if (_directions.every(data.hasAction)) directionalContainers++;
          candidate.visitChildren((child) {
            inspect(child);
            return true;
          });
        }

        inspect(
          tester
              .binding
              .renderViews
              .single
              .owner!
              .semanticsOwner!
              .rootSemanticsNode!,
        );
        expect(
          directionalContainers,
          1,
          reason: 'A loaded oversized 1:1 canvas needs real four-direction accessibility actions',
        );
        final node = _node(tester);
        for (final action in _directions) {
          expect(
            node.getSemanticsData().hasAction(action),
            isTrue,
            reason: '$action',
          );
        }
        expect(
          node.getSemanticsData().label,
          isEmpty,
          reason: 'The scroll container must not add a blank or technical reading stop',
        );
        expect(
          node.childrenCount,
          greaterThan(0),
          reason: 'Do not merge/exclude the actual contents',
        );
        expect(
          tester.getSize(
            find.byKey(const ValueKey('document-canvas:accessible-pan')),
          ),
          const Size(640, 1200),
        );
        expect(_controller(tester).value.getMaxScaleOnAxis(), 1);
      } finally {
        await tester.pumpWidget(const SizedBox.shrink());
        semantics.dispose();
      }
    },
    variant: platforms,
  );

  testWidgets(
    'semantic pan is viewport-sized bounded reversible and does not zoom',
    (tester) async {
      final semantics = tester.ensureSemantics();
      addTearDown(() => tester.binding.setSurfaceSize(null));
      try {
        await _load(tester, const Size(640, 1200));
        await _mode(tester, DemoCanvasScaleMode.actualSize);
        final controller = _controller(tester);
        final viewport = tester.getSize(find.byKey(_viewerKey));
        await _action(tester, SemanticsAction.scrollUp);
        expect(
          controller.value.storage[13],
          closeTo(-viewport.height * .8, .001),
        );
        expect(controller.value.storage[12], 0);
        await _action(tester, SemanticsAction.scrollDown);
        expect(controller.value.storage[13], closeTo(0, .001));
        await _action(tester, SemanticsAction.scrollLeft);
        expect(
          controller.value.storage[12],
          closeTo(-viewport.width * .8, .001),
        );
        await _action(tester, SemanticsAction.scrollRight);
        expect(controller.value.storage[12], closeTo(0, .001));
        final staleUp = tester
            .widget<Semantics>(find.byKey(_viewportKey))
            .properties
            .onScrollUp!;
        for (final action in _directions) {
          var count = 0;
          while (_node(tester).getSemanticsData().hasAction(action) &&
              count++ < 30) {
            await _action(tester, action);
          }
          expect(
            count,
            lessThan(30),
            reason: 'Each direction must reach a finite boundary',
          );
          expect(_node(tester).getSemanticsData().hasAction(action), isFalse);
          final translation = controller.value.getTranslation();
          expect(
            translation.x,
            inInclusiveRange(viewport.width - 640 - 80, 80),
          );
          expect(
            translation.y,
            inInclusiveRange(viewport.height - 1200 - 80, 80),
          );
          expect(controller.value.getMaxScaleOnAxis(), 1);
        }
        // Replayed native callbacks cannot cross the bound, even before a new frame.
        while (_node(tester)
            .getSemanticsData()
            .hasAction(SemanticsAction.scrollUp)) {
          await _action(tester, SemanticsAction.scrollUp);
        }
        final before = controller.value.clone();
        staleUp();
        staleUp();
        await tester.pumpAndSettle();
        expect(controller.value, before);
        final visible = tester.getRect(find.byKey(_viewerKey));
        final end = tester.getRect(
          find.byKey(const ValueKey('$_descriptor#end')),
        );
        expect(
          visible.overlaps(end),
          isTrue,
          reason: 'The end must become visible, not just change a matrix',
        );
      } finally {
        await tester.pumpWidget(const SizedBox.shrink());
        semantics.dispose();
      }
    },
    variant: platforms,
  );

  for (final entry in [
    (const Size(100, 100), false, false),
    (const Size(100, 1200), false, true),
    (const Size(1200, 100), true, false),
  ]) {
    testWidgets(
      'fit and non-overflow axes expose no false scroll: ${entry.$1}',
      (tester) async {
        final semantics = tester.ensureSemantics();
        addTearDown(() => tester.binding.setSurfaceSize(null));
        try {
          await _load(tester, entry.$1);
          for (final action in _directions) {
            expect(_node(tester).getSemanticsData().hasAction(action), isFalse);
          }
          await _mode(tester, DemoCanvasScaleMode.actualSize);
          final data = _node(tester).getSemanticsData();
          expect(data.hasAction(SemanticsAction.scrollLeft), entry.$2);
          expect(data.hasAction(SemanticsAction.scrollRight), entry.$2);
          expect(data.hasAction(SemanticsAction.scrollUp), entry.$3);
          expect(data.hasAction(SemanticsAction.scrollDown), entry.$3);
        } finally {
          await tester.pumpWidget(const SizedBox.shrink());
          semantics.dispose();
        }
      },
      variant: platforms,
    );
  }

  testWidgets(
    'pointer pan and rebuild retain position and update semantic bounds',
    (tester) async {
      final semantics = tester.ensureSemantics();
      addTearDown(() => tester.binding.setSurfaceSize(null));
      try {
        await _load(tester, const Size(640, 1200));
        await _mode(tester, DemoCanvasScaleMode.actualSize);
        final controller = _controller(tester);
        final viewer = find.byKey(_viewerKey);
        final before = controller.value.clone();
        await tester.dragFrom(
          tester.getRect(viewer).centerRight - const Offset(8, 0),
          const Offset(-45, -80),
        );
        await tester.pumpAndSettle();
        expect(
          controller.value,
          isNot(before),
          reason: 'Keep ordinary pan when adding semantics',
        );
        final panned = controller.value.clone();
        await _mode(tester, DemoCanvasScaleMode.fitViewport);
        for (final action in _directions) {
          expect(_node(tester).getSemanticsData().hasAction(action), isFalse);
        }
        await _mode(tester, DemoCanvasScaleMode.actualSize);
        expect(identical(_controller(tester), controller), isTrue);
        expect(
          controller.value,
          panned,
          reason: 'Mode/rebuild must not erase the reading position',
        );
        await _action(tester, SemanticsAction.scrollDown);
        expect(controller.value.storage[13], greaterThan(panned.storage[13]));
      } finally {
        await tester.pumpWidget(const SizedBox.shrink());
        semantics.dispose();
      }
    },
    variant: platforms,
  );
}
