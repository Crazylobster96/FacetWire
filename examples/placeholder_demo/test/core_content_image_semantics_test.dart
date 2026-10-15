// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

const _descriptor = 'image-semantics/root.dis.json';
const _pixel = 'image-semantics/pixel.png';
const _alternative = '合成绿色图片';
final _png = base64Decode(
  'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGUlEQVR4nGNQWJDwnxLMMGrAqAGjBgwXAwAOIh8fzdnvlAAAAABJRU5ErkJggg==',
);

Map<String, Object> _document(String type, Object? alt) => {
  'format': 'facetwire.agent-scene-package',
  'version': '0.1',
  'id': 'image-semantics',
  'title': 'Synthetic image semantics',
  'resources': [
    {'id': 'pixel', 'source': 'pixel.png', 'mediaType': 'image/png'},
  ],
  'canvas': {
    'size': {'width': 640, 'height': 400},
    'pages': [
      {
        'layers': [
          {
            'id': 'layer',
            'z': 0,
            'zones': [
              {
                'id': 'root-text',
                'bounds': {'x': 12, 'y': 10, 'width': 500, 'height': 70},
                'content': {
                  'type': 'text',
                  'text': 'selectable root',
                  'selectable': true,
                },
              },
              {
                'id': 'picture',
                'bounds': {'x': 12, 'y': 100, 'width': 160, 'height': 160},
                'content': {'type': type, 'resource': 'pixel', 'alt': ?alt},
              },
            ],
          },
        ],
      },
    ],
  },
};

class _ImageBundle extends CachingAssetBundle {
  _ImageBundle(this.document);
  final Map<String, Object> document;

  @override
  Future<ByteData> load(String key) async {
    if (key == 'AssetManifest.bin') {
      return const StandardMessageCodec().encodeMessage(<String, Object?>{})!;
    }
    if (key == _pixel) return ByteData.sublistView(_png);
    if (key != _descriptor) throw FlutterError('Unknown synthetic asset: $key');
    return ByteData.sublistView(
      Uint8List.fromList(utf8.encode(jsonEncode(document))),
    );
  }
}

void main() {
  for (final platform in [TargetPlatform.iOS, TargetPlatform.macOS]) {
    for (final type in ['image', 'animated-image']) {
      for (final alt in <Object?>[_alternative, null, '', 42]) {
        for (final localFile in [false, true]) {
          testWidgets('$platform $type alt=$alt localFile=$localFile', (
            tester,
          ) async {
            addTearDown(() => tester.binding.setSurfaceSize(null));
            await tester.binding.setSurfaceSize(const Size(1600, 1200));
            final semantics = tester.ensureSemantics();
            try {
              final document = _document(type, alt);
              final bundle = _ImageBundle(document);
              File? imageFile;
              String? path;
              if (localFile) {
                final temporary = Directory.systemTemp.createTempSync(
                  'facetwire-image-semantics-',
                );
                addTearDown(() => temporary.deleteSync(recursive: true));
                final package = Directory('${temporary.path}/synthetic.agscene')
                  ..createSync();
                final descriptor = File('${package.path}/root.dis.json')
                  ..writeAsStringSync(jsonEncode(document), encoding: utf8);
                imageFile = File('${package.path}/pixel.png')
                  ..writeAsBytesSync(_png);
                path = descriptor.path;
              }
              await tester.pumpWidget(
                DefaultAssetBundle(
                  bundle: bundle,
                  child: MaterialApp(
                    home: CoreContentDemoScreen(
                      loader: CoreContentPackageLoader(bundle: bundle),
                      descriptorAsset: _descriptor,
                      initialPath: path,
                    ),
                  ),
                ),
              );
              // File loading completes outside the widget test's fake clock.
              for (
                var attempt = 0;
                attempt < 40 && find.text('selectable root').evaluate().isEmpty;
                attempt++
              ) {
                await tester.runAsync(
                  () => Future<void>.delayed(const Duration(milliseconds: 20)),
                );
                await tester.pump(const Duration(milliseconds: 20));
              }
              await tester.pumpAndSettle();
              expect(find.text('selectable root'), findsOneWidget);
              final image = tester.widget<Image>(find.byType(Image));
              final zoneNode = tester.getSemantics(
                find.bySemanticsLabel(RegExp('$type: picture')),
              );
              expect(
                zoneNode.getSemanticsData().label,
                contains('$type: picture'),
              );
              final imageNode = tester.getSemantics(find.byType(Image));
              expect(
                imageNode.getSemanticsData().flagsCollection.isImage,
                isTrue,
              );
              if (alt == _alternative) {
                expect(image.semanticLabel, _alternative);
                expect(
                  imageNode.getSemanticsData().label,
                  contains(_alternative),
                );
              } else {
                expect(image.semanticLabel, anyOf(isNull, isEmpty));
                expect(
                  imageNode.getSemanticsData().label,
                  isNot(contains(_alternative)),
                );
              }
              // Labelling must not change Zone selection or selectable text.
              await tester.tap(find.byType(Image));
              await tester.pumpAndSettle();
              expect(find.text('type: $type'), findsOneWidget);
              await tester.tap(find.text('selectable root'));
              await tester.pumpAndSettle();
              expect(find.text('type: text'), findsOneWidget);
              if (imageFile != null) expect(imageFile.readAsBytesSync(), _png);
            } finally {
              await tester.pumpWidget(const SizedBox.shrink());
              semantics.dispose();
            }
          }, variant: TargetPlatformVariant({platform}));
        }
      }
    }
  }
}
