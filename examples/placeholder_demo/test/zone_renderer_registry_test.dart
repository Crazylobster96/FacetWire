// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test(
    'unknown type fails closed; duplicate or reserved registrations reject',
    () async {
      final root = await Directory.systemTemp.createTemp('facetwire-module-');
      addTearDown(() => root.delete(recursive: true));
      await _package(root);
      await expectLater(
        CoreContentPackageLoader().loadPath(root.path),
        throwsFormatException,
      );
      final module = _module();
      expect(
        () => CoreContentPackageLoader(renderers: [module, module]),
        throwsArgumentError,
      );
      expect(
        () => CoreContentPackageLoader(
          renderers: [
            FacetWireZoneRenderer(
              type: 'document',
              validate: module.validate,
              build: module.build,
            ),
          ],
        ),
        throwsArgumentError,
      );
      expect(
        () => CoreContentPackageLoader(
          renderers: [
            FacetWireZoneRenderer(
              type: '../bad',
              validate: module.validate,
              build: module.build,
            ),
          ],
        ),
        throwsArgumentError,
      );
    },
  );

  testWidgets(
    'a separately registered module validates and draws a package zone',
    (tester) async {
      var validations = 0;
      final module = FacetWireZoneRenderer(
        type: 'third-party-tile',
        validate: (content, resources) {
          validations += 1;
          if (content['resource'] != 'sample' ||
              !resources.containsKey('sample')) {
            throw const FormatException('module resource unavailable');
          }
        },
        build: (context, zone) => Text(
          'Module rendered: ${zone.id}: ${zone.resources['sample'] != null}',
        ),
      );
      final loader = CoreContentPackageLoader(
        bundle: _MemoryBundle({
          'fixture/custom.dis.json': utf8.encode(jsonEncode(_descriptor())),
          'fixture/resources/sample.bin': [1, 2, 3],
        }),
        renderers: [module],
      );
      final document = await loader.load('fixture/custom.dis.json');
      expect(document.zones.single.type, 'third-party-tile');
      expect(validations, 1);
      await tester.pumpWidget(
        MaterialApp(
          home: CoreContentDemoScreen(
            loader: loader,
            descriptorAsset: 'fixture/custom.dis.json',
            renderers: [module],
          ),
        ),
      );
      await tester.pump();
      expect(
        find.textContaining('Module rendered: custom-zone: true'),
        findsOneWidget,
      );
      expect(validations, 2);
    },
  );

  test(
    'module validation failure rejects before a widget can be shown',
    () async {
      final root = await Directory.systemTemp.createTemp('facetwire-module-');
      addTearDown(() => root.delete(recursive: true));
      await _package(root);
      final module = FacetWireZoneRenderer(
        type: 'third-party-tile',
        validate: (content, resources) => throw const FormatException('denied'),
        build: (context, zone) => const Text('must not render'),
      );
      await expectLater(
        CoreContentPackageLoader(renderers: [module]).loadPath(root.path),
        throwsFormatException,
      );
    },
  );

  testWidgets(
    'module builder failure stays inside a visible unavailable zone',
    (tester) async {
      final module = FacetWireZoneRenderer(
        type: 'third-party-tile',
        validate: (content, resources) {},
        build: (context, zone) =>
            throw StateError('synthetic renderer failure'),
      );
      final loader = CoreContentPackageLoader(
        bundle: _MemoryBundle({
          'fixture/custom.dis.json': utf8.encode(jsonEncode(_descriptor())),
          'fixture/resources/sample.bin': [1, 2, 3],
        }),
        renderers: [module],
      );
      await tester.pumpWidget(
        MaterialApp(
          home: CoreContentDemoScreen(
            loader: loader,
            descriptorAsset: 'fixture/custom.dis.json',
            renderers: [module],
          ),
        ),
      );
      await tester.pump();
      expect(find.text('Renderer unavailable'), findsOneWidget);
      expect(
        find.byKey(const ValueKey('document-canvas:custom-document')),
        findsOneWidget,
      );
    },
  );
}

FacetWireZoneRenderer _module() => FacetWireZoneRenderer(
  type: 'third-party-tile',
  validate: (content, resources) {},
  build: (context, zone) => const Text('separate module'),
);

Future<void> _package(Directory root) async {
  final resource = Directory('${root.path}${Platform.pathSeparator}resources');
  await resource.create();
  await File('${resource.path}${Platform.pathSeparator}sample.bin')
      .writeAsBytes([1, 2, 3]);
  await File('${root.path}${Platform.pathSeparator}custom.dis.json')
      .writeAsString(jsonEncode(_descriptor()), encoding: utf8);
}

Map<String, Object?> _descriptor() => {
  'format': 'facetwire.agent-scene-package',
  'version': '0.1',
  'id': 'custom-document',
  'title': 'Module fixture',
  'resources': [
    {
      'id': 'sample',
      'source': 'resources/sample.bin',
      'mediaType': 'application/octet-stream',
    },
  ],
  'canvas': {
    'size': {'width': 400, 'height': 300},
    'pages': [
      {
        'layers': [
          {
            'id': 'layer',
            'z': 0,
            'zones': [
              {
                'id': 'custom-zone',
                'bounds': {'x': 10, 'y': 10, 'width': 180, 'height': 100},
                'content': {'type': 'third-party-tile', 'resource': 'sample'},
              },
            ],
          },
        ],
      },
    ],
  },
};

final class _MemoryBundle extends CachingAssetBundle {
  _MemoryBundle(this.files);

  final Map<String, List<int>> files;

  @override
  Future<ByteData> load(String key) async {
    final bytes = files[key];
    if (bytes == null) throw FlutterError('missing fixture asset: $key');
    return ByteData.sublistView(Uint8List.fromList(bytes));
  }
}
