// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/demo_app.dart';
import 'package:facetwire_placeholder_demo/src/package_loader.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('loads the conforming three-level recursive package', () async {
    final document = await AgscenePackageLoader.asset().load(
      demoDescriptorAsset,
    );

    expect(document.documents.map((item) => item.depth), [1, 2, 3]);
    expect(document.placeholderZones.map((zone) => zone.kind), [
      'image',
      'chart',
      'video',
    ]);
    expect(document.documents.map((item) => item.resourceCount), [1, 1, 1]);

    final level2Zone = document.zones.singleWhere(
      (zone) => zone.id == 'zone:nested-level-2',
    );
    expect(level2Zone.child?.depth, 2);
    expect(level2Zone.documentPlacement?.fit, SceneDocumentFit.none);
    expect(level2Zone.documentPlacement?.alignmentX, 0);
    expect(level2Zone.documentPlacement?.alignmentY, 0);
    expect(level2Zone.documentPlacement?.clip, isFalse);
    expect(level2Zone.bounds.width, level2Zone.child?.canvasWidth);
    expect(level2Zone.bounds.height, level2Zone.child?.canvasHeight);

    final level3Zone = level2Zone.child!.zones.singleWhere(
      (zone) => zone.id == 'zone:nested-level-3',
    );
    expect(level3Zone.child?.depth, 3);
    expect(level3Zone.documentPlacement?.fit, SceneDocumentFit.none);
    expect(level3Zone.documentPlacement?.clip, isFalse);
    expect(level3Zone.bounds.width, 1024);
    expect(level3Zone.bounds.height, 576);
    expect(level3Zone.bounds.width, level3Zone.child?.canvasWidth);
    expect(level3Zone.child?.canvasWidth, 1024);
    expect(level3Zone.child?.canvasHeight, 576);
  });

  test('rejects a package-relative path that escapes its package', () async {
    final descriptor = <String, Object?>{
      'format': 'facetwire.agent-scene-package',
      'version': '0.1',
      'id': 'document:unsafe',
      'title': 'Unsafe',
      'canvas': {
        'id': 'canvas:unsafe',
        'size': {'width': 100, 'height': 100},
        'pages': [
          {
            'id': 'page:unsafe',
            'size': {'width': 100, 'height': 100},
            'layers': [
              {
                'id': 'layer:unsafe',
                'z': 0,
                'zones': [
                  {
                    'id': 'zone:unsafe',
                    'bounds': {'x': 0, 'y': 0, 'width': 10, 'height': 10},
                    'content': {
                      'type': 'document',
                      'source': '../escape.agscene',
                    },
                  },
                ],
              },
            ],
          },
        ],
      },
      'resources': <Object?>[],
    };
    final loader = AgscenePackageLoader((path) async => jsonEncode(descriptor));

    expect(
      () => loader.load('unsafe.agscene/unsafe.agscene.dis.json'),
      throwsFormatException,
    );
  });

  test('enforces the configured recursion depth', () async {
    final loader = AgscenePackageLoader.asset(maxDepth: 2);
    expect(() => loader.load(demoDescriptorAsset), throwsFormatException);
  });
}
