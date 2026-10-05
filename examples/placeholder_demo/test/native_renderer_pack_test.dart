// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:facetwire_placeholder_demo/src/native_renderer_pack.dart';

const _bridgePath = String.fromEnvironment('FW_FLUTTER_BRIDGE_TEST_PATH');
const _pluginPath = String.fromEnvironment('FW_FLUTTER_PLUGIN_TEST_PATH');

Future<String> digest(String path) async =>
    (await sha256.bind(File(path).openRead()).first).toString();

void main() {
  test(
    'native plugin source requires exact identity and two pinned files',
    () async {
      await expectLater(
        NativeRendererPack.load(
          bridgeLibraryPath: 'relative.dll',
          bridgeSha256: '0' * 64,
          pluginLibraryPath: 'relative-plugin.dll',
          pluginSha256: '0' * 64,
          expectedPluginId: 'org.facetwire.reference.status-tile-renderer',
        ),
        throwsFormatException,
      );
      await expectLater(
        NativeRendererPack.load(
          bridgeLibraryPath: 'relative.dll',
          bridgeSha256: 'wrong',
          pluginLibraryPath: 'relative-plugin.dll',
          pluginSha256: '0' * 64,
          expectedPluginId: 'org.facetwire.reference.status-tile-renderer',
        ),
        throwsFormatException,
      );
      await expectLater(
        NativeRendererPack.load(
          bridgeLibraryPath: 'relative.dll',
          bridgeSha256: '0' * 64,
          pluginLibraryPath: 'relative-plugin.dll',
          pluginSha256: '0' * 64,
          expectedPluginId: 'not a plugin id',
        ),
        throwsFormatException,
      );
    },
  );

  testWidgets('native plugin profile reaches Flutter zone rendering', (
    tester,
  ) async {
    if (_bridgePath.isEmpty || _pluginPath.isEmpty) return;
    final loaded = await tester.runAsync(() async {
      final bridgeSha = await digest(_bridgePath);
      final pluginSha = await digest(_pluginPath);
      final pack = await NativeRendererPack.load(
        bridgeLibraryPath: _bridgePath,
        bridgeSha256: bridgeSha,
        pluginLibraryPath: _pluginPath,
        pluginSha256: pluginSha,
        expectedPluginId: 'org.facetwire.reference.status-tile-renderer',
      );
      await expectLater(
        NativeRendererPack.load(
          bridgeLibraryPath: _bridgePath,
          bridgeSha256: bridgeSha,
          pluginLibraryPath: _pluginPath,
          pluginSha256: '0' * 64,
          expectedPluginId: 'org.facetwire.reference.status-tile-renderer',
        ),
        throwsFormatException,
      );
      await expectLater(
        NativeRendererPack.load(
          bridgeLibraryPath: _bridgePath,
          bridgeSha256: bridgeSha,
          pluginLibraryPath: _pluginPath,
          pluginSha256: pluginSha,
          expectedPluginId: 'org.facetwire.test.other',
        ),
        throwsFormatException,
      );
      return pack;
    });
    expect(loaded, isNotNull);
    expect(loaded!.type, 'status-tile');
    final renderer = loaded.asRenderer();
    expect(renderer.type, 'status-tile');
    const content = <String, Object?>{
      'type': 'status-tile',
      'label': 'Native renderer profile',
      'foreground': '#112233ff',
    };
    renderer.validate(content, const {});
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Builder(
            builder: (context) => renderer.build(
              context,
              const DemoZone(
                key: 'zone',
                id: 'zone',
                bounds: DemoBounds(0, 0, 100, 40),
                content: content,
                resourceAsset: null,
                posterAsset: null,
                child: null,
                documentFit: 'contain',
              ),
            ),
          ),
        ),
      ),
    );
    expect(find.text('Native renderer profile'), findsOneWidget);
  });
}
