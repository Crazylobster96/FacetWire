// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';
import 'dart:ui' show SemanticsAction;

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart' show SemanticsNode;
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

const _descriptor = 'image-semantics/root.dis.json';
const _pixel = 'image-semantics/pixel.png';
const _alternative = '合成绿色图片';
final _png = base64Decode(
  'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGUlEQVR4nGNQWJDwnxLMMGrAqAGjBgwXAwAOIh8fzdnvlAAAAABJRU5ErkJggg==',
);

int _tapDescendants(SemanticsNode node) {
  var count = 0;
  node.visitChildren((child) {
    if (child.getSemanticsData().hasAction(SemanticsAction.tap)) count++;
    count += _tapDescendants(child);
    return true;
  });
  return count;
}

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
  _ImageBundle(this.document, {this.broken = false});
  final Map<String, Object> document;
  final bool broken;

  @override
  Future<ByteData> load(String key) async {
    if (key == 'AssetManifest.bin') {
      return const StandardMessageCodec().encodeMessage(<String, Object?>{})!;
    }
    if (key == _pixel) {
      return ByteData.sublistView(
        broken ? Uint8List.fromList([0, 1, 2]) : _png,
      );
    }
    if (key != _descriptor) throw FlutterError('Unknown synthetic asset: $key');
    return ByteData.sublistView(
      Uint8List.fromList(utf8.encode(jsonEncode(document))),
    );
  }
}

void main() {
  for (final platform in [TargetPlatform.iOS, TargetPlatform.macOS]) {
    for (final type in ['image', 'animated-image']) {
      testWidgets('$platform $type keeps resource failure readable', (
        tester,
      ) async {
        final semantics = tester.ensureSemantics();
        final bundle = _ImageBundle(
          _document(type, _alternative),
          broken: true,
        );
        try {
          await tester.pumpWidget(
            DefaultAssetBundle(
              bundle: bundle,
              child: MaterialApp(
                home: CoreContentDemoScreen(
                  loader: CoreContentPackageLoader(bundle: bundle),
                  descriptorAsset: _descriptor,
                ),
              ),
            ),
          );
          await tester.pumpAndSettle();
          expect(find.text('Resource unavailable\npicture'), findsOneWidget);
          final node = tester.getSemantics(
            find.text('Resource unavailable\npicture'),
          );
          expect(
            node.getSemanticsData().label,
            contains('Resource unavailable'),
          );
          // Without the redundant gesture boundary the error text may join
          // the owning description; both must remain available.
          final described = find.bySemanticsLabel(RegExp(_alternative));
          expect(described, findsOneWidget);
          final describedNode = tester.getSemantics(described);
          expect(
            describedNode.getSemanticsData().label,
            contains(_alternative),
          );
          expect(
            describedNode.getSemanticsData().flagsCollection.isImage,
            isTrue,
          );
          expect(
            describedNode.getSemanticsData().hasAction(SemanticsAction.tap),
            isTrue,
          );
          expect(
            _tapDescendants(describedNode),
            0,
            reason:
                'Decode failure must not create a blank duplicate image action',
          );
        } finally {
          await tester.pumpWidget(const SizedBox.shrink());
          semantics.dispose();
        }
      }, variant: TargetPlatformVariant({platform}));
    }
  }
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
                alt == _alternative
                    ? find.bySemanticsLabel(_alternative)
                    : find.bySemanticsLabel(RegExp('$type: picture')),
              );
              if (alt != _alternative) {
                expect(
                  zoneNode.getSemanticsData().label,
                  contains('$type: picture'),
                );
              }
              final imageNode = tester.getSemantics(
                alt == _alternative
                    ? find.bySemanticsLabel(_alternative)
                    : find.byType(Image),
              );
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
                expect(
                  imageNode.getSemanticsData().label,
                  isNot(contains('picture')),
                );
                expect(
                  imageNode.getSemanticsData().label,
                  isNot(contains('1.00')),
                );
                expect(
                  imageNode.getSemanticsData().hasAction(SemanticsAction.tap),
                  isTrue,
                  reason: 'The image description and Zone activation must share a node',
                );
                var descriptions = 0;
                void countDescriptions(SemanticsNode node) {
                  if (node.getSemanticsData().label == _alternative) {
                    descriptions++;
                  }
                  node.visitChildren((child) {
                    countDescriptions(child);
                    return true;
                  });
                }

                countDescriptions(imageNode.owner!.rootSemanticsNode!);
                expect(
                  descriptions,
                  1,
                  reason: 'No competing duplicate description node',
                );
                expect(
                  _tapDescendants(imageNode),
                  0,
                  reason: 'The described image must not have a competing blank tap child',
                );
                imageNode.owner!.performAction(
                  imageNode.id,
                  SemanticsAction.tap,
                );
                await tester.pumpAndSettle();
                expect(find.text('type: $type'), findsOneWidget);
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
  testWidgets(
    'Custom image renderer keeps its independent controls and semantics',
    (tester) async {
      final semantics = tester.ensureSemantics();
      var pressed = false;
      final bundle = _ImageBundle(_document('image', _alternative));
      final renderer = FacetWireZoneRenderer(
        type: 'image',
        validate: (_, _) {},
        build: (_, _) => Column(
          children: [
            const Text('Custom image description'),
            TextButton(
              onPressed: () => pressed = true,
              child: const Text('Custom action'),
            ),
          ],
        ),
      );
      try {
        await tester.pumpWidget(
          DefaultAssetBundle(
            bundle: bundle,
            child: MaterialApp(
              home: CoreContentDemoScreen(
                loader: CoreContentPackageLoader(
                  bundle: bundle,
                  renderers: [renderer],
                ),
                descriptorAsset: _descriptor,
                renderers: [renderer],
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(
          find.bySemanticsLabel(RegExp('Custom image description')),
          findsOneWidget,
        );
        final action = tester.getSemantics(find.text('Custom action'));
        expect(
          action.getSemanticsData().hasAction(SemanticsAction.tap),
          isTrue,
        );
        action.owner!.performAction(action.id, SemanticsAction.tap);
        await tester.pumpAndSettle();
        expect(pressed, isTrue);
      } finally {
        await tester.pumpWidget(const SizedBox.shrink());
        semantics.dispose();
      }
    },
  );
}
