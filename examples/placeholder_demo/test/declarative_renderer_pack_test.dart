// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:facetwire_placeholder_demo/src/declarative_renderer_pack.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final declaration = {
    'schema': 'facetwire.declarative-zone.v1',
    'type': 'status-tile',
    'view': {
      'kind': 'box',
      'colorField': 'background',
      'child': {'kind': 'text', 'field': 'label', 'colorField': 'foreground'},
    },
  };

  DeclarativeRendererPack pack(Object value) =>
      DeclarativeRendererPack.parse(utf8.encode(jsonEncode(value)));
  final digest = sha256
      .convert(utf8.encode(jsonEncode(declaration)))
      .toString();

  testWidgets('separately loaded data-only renderer draws a custom zone', (
    tester,
  ) async {
    final module = pack(declaration).asRenderer();
    final loader = CoreContentPackageLoader(
      bundle: _MemoryBundle({
        'fixture/custom.dis.json': utf8.encode(jsonEncode(_document())),
      }),
      renderers: [module],
    );
    final document = await loader.load('fixture/custom.dis.json');
    expect(document.zones.single.type, 'status-tile');
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
    expect(find.text('独立插件显示'), findsOneWidget);
    final text = tester.widget<Text>(find.text('独立插件显示'));
    expect(text.style?.color, const Color(0xff112233));
  });

  test(
    'pack grammar and byte limits reject unsupported or ambiguous forms',
    () {
      expect(() => DeclarativeRendererPack.parse([]), throwsFormatException);
      expect(
        () => DeclarativeRendererPack.parse(List.filled(65537, 32)),
        throwsFormatException,
      );
      expect(
        () => DeclarativeRendererPack.parse([0xff]),
        throwsFormatException,
      );
      expect(
        () => pack({...declaration, 'type': 'document'}),
        throwsFormatException,
      );
      expect(
        () => pack({...declaration, 'extra': true}),
        throwsFormatException,
      );
      expect(
        () => pack({
          ...declaration,
          'view': {'kind': 'native-code'},
        }),
        throwsFormatException,
      );
      expect(
        () => pack({
          ...declaration,
          'view': {'kind': 'text', 'field': '../x', 'colorField': 'foreground'},
        }),
        throwsFormatException,
      );
      expect(
        () => pack({
          ...declaration,
          'view': {'kind': 'column', 'children': []},
        }),
        throwsFormatException,
      );
      Object tree = {
        'kind': 'text',
        'field': 'label',
        'colorField': 'foreground',
      };
      for (var index = 0; index < 9; index++) {
        tree = {'kind': 'box', 'colorField': 'background', 'child': tree};
      }
      expect(() => pack({...declaration, 'view': tree}), throwsFormatException);
    },
  );

  test('content and resource references fail before widget construction', () {
    final renderer = pack(declaration).asRenderer();
    final valid = <String, Object?>{
      'type': 'status-tile',
      'label': 'ready',
      'foreground': '#112233ff',
      'background': '#eeeeeeff',
    };
    renderer.validate(valid, const {});
    for (final content in [
      {...valid, 'type': 'other'},
      {...valid, 'label': ''},
      {...valid, 'foreground': 'red'},
      {...valid, 'unknown': 'ignored'},
      {...valid, 'label': 'x' * 2049},
    ]) {
      expect(() => renderer.validate(content, const {}), throwsFormatException);
    }
    expect(
      () => renderer.validate(
        valid,
        Map.fromEntries(
          List.generate(257, (index) => MapEntry('$index', 'resource')),
        ),
      ),
      throwsFormatException,
    );
  });

  test('load refuses relative, missing and non-regular pack files', () async {
    await expectLater(
      DeclarativeRendererPack.load(
        'relative.renderer.json',
        expectedSha256: digest,
      ),
      throwsFormatException,
    );
    final temp = await Directory.systemTemp.createTemp(
      'facetwire-render-pack-',
    );
    addTearDown(() => temp.delete(recursive: true));
    await expectLater(
      DeclarativeRendererPack.load(
        '${temp.path}${Platform.pathSeparator}missing.json',
        expectedSha256: digest,
      ),
      throwsFormatException,
    );
    await expectLater(
      DeclarativeRendererPack.load(temp.path, expectedSha256: digest),
      throwsFormatException,
    );
    final valid = File('${temp.path}${Platform.pathSeparator}renderer.json');
    await valid.writeAsString(jsonEncode(declaration), encoding: utf8);
    expect(
      (await DeclarativeRendererPack.load(
        valid.absolute.path,
        expectedSha256: digest,
      )).type,
      'status-tile',
    );
    await expectLater(
      DeclarativeRendererPack.load(
        valid.absolute.path,
        expectedSha256: '0' * 64,
      ),
      throwsFormatException,
    );
  });
}

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

Map<String, Object?> _document() => {
  'format': 'facetwire.agent-scene-package',
  'version': '0.1',
  'id': 'custom-document',
  'title': 'Runtime pack fixture',
  'resources': [],
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
                'content': {
                  'type': 'status-tile',
                  'label': '独立插件显示',
                  'foreground': '#112233ff',
                  'background': '#eeeeeeff',
                },
              },
            ],
          },
        ],
      },
    ],
  },
};
