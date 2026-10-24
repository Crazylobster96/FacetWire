// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ui' show PointerDeviceKind;

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

const _root = 'selection/root.dis.json';
const _child = 'selection/child.dis.json';
const _pixel = 'selection/pixel.png';

Map<String, Object> _text(String id, String text, bool selectable) => {
  'id': id,
  'bounds': {'x': 8, 'y': 8, 'width': 450, 'height': 70},
  'content': {'type': 'text', 'text': text, 'selectable': selectable},
};

Map<String, Object> _document(
  String id,
  List<Object> zones, {
  bool child = false,
}) => {
  'format': 'facetwire.agent-scene-package',
  'version': '0.1',
  'id': id,
  'title': id,
  'resources': child
      ? <Object>[]
      : [
          {'id': 'pixel', 'source': 'pixel.png', 'mediaType': 'image/png'},
        ],
  'canvas': {
    'size': {'width': child ? 480 : 640, 'height': child ? 180 : 1200},
    'pages': [
      {
        'layers': [
          {'id': 'layer', 'z': 0, 'zones': zones},
        ],
      },
    ],
  },
};

class _SelectionBundle extends CachingAssetBundle {
  _SelectionBundle(bool selectable)
    : files = {
        _root: _document('root', [
          _text('root-zone', 'root text selection', selectable),
          for (final (id, y) in [('first-child', 110), ('second-child', 330)])
            {
              'id': id,
              'bounds': {'x': 12, 'y': y, 'width': 480, 'height': 180},
              'content': {'type': 'document', 'source': 'child.dis.json'},
            },
          {
            'id': 'picture',
            'bounds': {'x': 12, 'y': 550, 'width': 160, 'height': 160},
            'content': {'type': 'image', 'resource': 'pixel'},
          },
        ]),
        _child: _document('shared-child', [
          _text('child-zone', 'shared text selection', selectable),
        ], child: true),
      };

  final Map<String, Object> files;

  @override
  Future<ByteData> load(String key) async {
    if (key == 'AssetManifest.bin') {
      return const StandardMessageCodec().encodeMessage(<String, Object?>{})!;
    }
    if (key == _pixel) {
      // Original synthetic 16x16 PNG; no external fixture or network resource.
      return ByteData.sublistView(
        base64Decode(
          'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGUlEQVR4nGNQWJDwnxLMMGrAqAGjBgwXAwAOIh8fzdnvlAAAAABJRU5ErkJggg==',
        ),
      );
    }
    final descriptor = files[key];
    if (descriptor == null) throw FlutterError('Unknown synthetic asset: $key');
    return ByteData.sublistView(
      Uint8List.fromList(utf8.encode(jsonEncode(descriptor))),
    );
  }
}

double _border(WidgetTester tester, Finder zone) =>
    ((tester
                        .widget<DecoratedBox>(
                          find
                              .descendant(
                                of: zone,
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

void main() {
  for (final (platform, kind) in [
    (TargetPlatform.macOS, PointerDeviceKind.mouse),
    (TargetPlatform.iOS, PointerDeviceKind.touch),
  ]) {
    for (final selectable in [false, true]) {
      for (final mode in ['适应窗口', '固定 1:1']) {
        testWidgets(
          '$platform $mode selectable=$selectable reselects root and each child',
          (tester) async {
            addTearDown(() => tester.binding.setSurfaceSize(null));
            await tester.binding.setSurfaceSize(const Size(1600, 1600));
            final bundle = _SelectionBundle(selectable);
            await tester.pumpWidget(
              DefaultAssetBundle(
                bundle: bundle,
                child: MaterialApp(
                  home: CoreContentDemoScreen(
                    loader: CoreContentPackageLoader(bundle: bundle),
                    descriptorAsset: _root,
                  ),
                ),
              ),
            );
            await tester.pumpAndSettle();
            await tester.ensureVisible(find.text(mode));
            await tester.tap(find.text(mode), kind: kind);
            await tester.pumpAndSettle();

            final root = find.byKey(const ValueKey('$_root#root-zone'));
            final children = find.byKey(const ValueKey('$_child#child-zone'));
            final picture = find.byKey(const ValueKey('$_root#picture'));
            expect(children, findsNWidgets(2));
            expect(
              find.descendant(
                of: find.byKey(const ValueKey('document-canvas:root')),
                matching: find.byType(SelectableText),
              ),
              selectable ? findsNWidgets(3) : findsNothing,
            );

            Future<void> selectPicture() async {
              await tester.tapAt(tester.getRect(picture).center, kind: kind);
              await tester.pump(const Duration(milliseconds: 100));
              expect(find.text('type: image'), findsOneWidget);
              expect(_border(tester, picture), 4);
            }

            Future<void> selectText(Finder zone) async {
              final text = find
                  .descendant(
                    of: zone,
                    matching: find.byType(selectable ? SelectableText : Text),
                  )
                  .first;
              final rect = tester.getRect(text);
              final scale = rect.width / tester.getSize(text).width;
              // Hit actual glyphs, not an empty part of the outer Zone.
              await tester.tapAt(
                rect.topLeft + Offset(20 * scale, 10 * scale),
                kind: kind,
              );
              await tester.pump(const Duration(milliseconds: 100));
              expect(find.text('type: text'), findsOneWidget);
              expect(_border(tester, zone), 4);
              expect(_border(tester, picture), 1.5);
            }

            await selectPicture();
            await selectText(root);
            // Repeating selection is idempotent, not a deselect toggle.
            await selectText(root);
            for (var index = 0; index < 2; index++) {
              await selectPicture();
              await selectText(children.at(index));
              expect(_border(tester, children.at(1 - index)), 1.5);
            }
            if (selectable) {
              await selectText(root);
              String? copied;
              tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
                SystemChannels.platform,
                (call) async {
                  if (call.method == 'Clipboard.setData') {
                    copied =
                        (call.arguments as Map<Object?, Object?>)['text']
                            as String;
                  }
                  return null;
                },
              );
              addTearDown(
                () => tester.binding.defaultBinaryMessenger
                    .setMockMethodCallHandler(SystemChannels.platform, null),
              );
              final editable = tester.state<EditableTextState>(
                find.descendant(of: root, matching: find.byType(EditableText)),
              );
              expect(editable.widget.enableInteractiveSelection, isTrue);
              expect(editable.widget.readOnly, isTrue);
              editable.selectAll(SelectionChangedCause.toolbar);
              await tester.pump();
              expect(
                editable.widget.controller.selection.textInside(
                  editable.widget.controller.text,
                ),
                'root text selection',
              );
              editable.copySelection(SelectionChangedCause.toolbar);
              await tester.pump();
              expect(copied, 'root text selection');
              expect(_border(tester, root), 4);
            }
          },
          variant: TargetPlatformVariant({platform}),
        );
      }
    }
  }
}
