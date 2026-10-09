// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

const _descriptor = 'synthetic/root.dis.json';
const _childDescriptor = 'synthetic/child.dis.json';
const _canvasSize = Size(640, 1200);

Map<String, Object> _textZone(String id, double y) => {
  'id': id,
  'bounds': {'x': 12, 'y': y, 'width': 480, 'height': 70},
  'content': {'type': 'text', 'text': id},
};

Map<String, Object> _document(String id, Size size, List<Object> zones) => {
  'format': 'facetwire.agent-scene-package',
  'version': '0.1',
  'id': id,
  'title': id,
  'resources': <Object>[],
  'canvas': {
    'size': {'width': size.width, 'height': size.height},
    'pages': [
      {
        'layers': [
          {'id': 'layer', 'z': 0, 'zones': zones},
        ],
      },
    ],
  },
};

class _SyntheticBundle extends CachingAssetBundle {
  final _files = {
    _descriptor: _document('viewport-root', _canvasSize, [
      _textZone('start', 10),
      for (final (id, y) in [('first-child', 110), ('second-child', 330)])
        {
          'id': id,
          'bounds': {'x': 12, 'y': y, 'width': 480, 'height': 180},
          'content': {'type': 'document', 'source': 'child.dis.json'},
        },
      _textZone('end', 1050),
    ]),
    _childDescriptor: _document('shared-child', const Size(480, 180), [
      _textZone('shared-text', 8),
    ]),
  };

  @override
  Future<ByteData> load(String key) async {
    final descriptor = _files[key];
    if (descriptor == null) throw FlutterError('Missing synthetic asset: $key');
    return ByteData.sublistView(
      Uint8List.fromList(utf8.encode(jsonEncode(descriptor))),
    );
  }
}

void main() {
  testWidgets(
    'fit scales paint once without shrinking root or shared children',
    (tester) async {
      addTearDown(() => tester.binding.setSurfaceSize(null));
      await tester.binding.setSurfaceSize(const Size(375, 640));
      await tester.pumpWidget(
        MaterialApp(
          home: CoreContentDemoScreen(
            loader: CoreContentPackageLoader(bundle: _SyntheticBundle()),
            descriptorAsset: _descriptor,
          ),
        ),
      );
      await tester.pumpAndSettle();

      final root = find.byKey(const ValueKey('document-canvas:viewport-root'));
      final outer = find.byKey(const ValueKey('preview-canvas-box'));
      final children = find.byKey(
        const ValueKey('document-canvas:shared-child'),
      );
      final end = find.byKey(const ValueKey('$_descriptor#end'));
      expect(children, findsNWidgets(2));
      for (final surface in [
        const Size(375, 640),
        const Size(800, 600),
        const Size(1400, 900),
        const Size(1600, 1600),
      ]) {
        await tester.binding.setSurfaceSize(surface);
        await tester.pumpAndSettle();
        expect(
          tester.getSize(root),
          _canvasSize,
          reason: '$surface root layout',
        );
        for (final child in children.evaluate()) {
          expect(child.size, const Size(480, 180));
        }
        final outerRect = tester.getRect(outer);
        final rootRect = tester.getRect(root);
        expect(rootRect.left, closeTo(outerRect.left, 0.001));
        expect(rootRect.top, closeTo(outerRect.top, 0.001));
        expect(rootRect.width, closeTo(outerRect.width, 0.001));
        expect(rootRect.height, closeTo(outerRect.height, 0.001));
        final scale = outerRect.width / _canvasSize.width;
        final endRect = tester.getRect(end);
        // The bottom Zone must be painted within the fitted root, not clipped by
        // an already scaled layout and then painted with a second scale.
        expect(endRect.top, closeTo(rootRect.top + 1050 * scale, 0.001));
        expect(endRect.bottom, lessThanOrEqualTo(rootRect.bottom));
        final start = find.byKey(const ValueKey('$_descriptor#start'));
        await tester.tapAt(tester.getRect(start).center);
        await tester.pump();
        double endBorderWidth() =>
            ((tester
                                .widget<DecoratedBox>(
                                  find
                                      .descendant(
                                        of: end,
                                        matching: find.byType(DecoratedBox),
                                      )
                                      .first,
                                )
                                .decoration
                            as BoxDecoration)
                        .border!
                    as Border)
                .top
                .width;
        expect(endBorderWidth(), 1.5);
        await tester.tapAt(endRect.center);
        await tester.pump();
        expect(endBorderWidth(), 4); // Selected by the transformed hit target.
      }
    },
  );

  testWidgets('actual size and fit transitions keep declared geometry', (
    tester,
  ) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(375, 640));
    await tester.pumpWidget(
      MaterialApp(
        home: CoreContentDemoScreen(
          loader: CoreContentPackageLoader(bundle: _SyntheticBundle()),
          descriptorAsset: _descriptor,
        ),
      ),
    );
    await tester.pumpAndSettle();
    final root = find.byKey(const ValueKey('document-canvas:viewport-root'));
    final outer = find.byKey(const ValueKey('preview-canvas-box'));
    for (final label in ['固定 1:1', '适应窗口', '固定 1:1']) {
      await tester.ensureVisible(find.text(label));
      await tester.tap(find.text(label));
      await tester.pumpAndSettle();
      expect(tester.getSize(root), _canvasSize);
      final viewer = tester.widget<InteractiveViewer>(
        find.byKey(const ValueKey('canvas-interactive-viewer')),
      );
      final actual = label == '固定 1:1';
      expect(viewer.panEnabled, actual);
      expect(viewer.scaleEnabled, !actual);
      if (actual) {
        expect(tester.getSize(outer), _canvasSize);
        expect(tester.getRect(root).size, _canvasSize);
      } else {
        expect(tester.getSize(outer).height, lessThan(_canvasSize.height));
      }
    }
  });
}
