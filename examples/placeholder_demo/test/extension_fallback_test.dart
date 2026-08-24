// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/demo_models.dart';
import 'package:facetwire_placeholder_demo/src/package_loader.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test(
    'routes unknown extension content to placeholder and probes bytes',
    () async {
      const descriptorPath = 'extension.agscene/extension.agscene.dis.json';
      String? probedPath;
      final descriptor = <String, Object?>{
        'format': 'facetwire.agent-scene-package',
        'version': '0.1',
        'id': 'document:extension',
        'title': 'Extension fixture',
        'canvas': {
          'id': 'canvas:extension',
          'size': {'width': 100, 'height': 100},
          'pages': [
            {
              'id': 'page:extension',
              'size': {'width': 100, 'height': 100},
              'layers': [
                {
                  'id': 'layer:extension',
                  'z': 0,
                  'zones': [
                    {
                      'id': 'zone:extension',
                      'bounds': {'x': 0, 'y': 0, 'width': 80, 'height': 60},
                      'content': {
                        'type': 'vendor.example.binary-widget',
                        'source': 'resources/widget.bin',
                      },
                    },
                  ],
                },
              ],
            },
          ],
        },
        'resources': [
          {
            'id': 'resource:binary',
            'source': 'resources/widget.bin',
            'mediaType': 'application/octet-stream',
          },
        ],
      };
      final loader = AgscenePackageLoader((path) async {
        if (path != descriptorPath) {
          throw StateError('Binary resource must not be decoded as UTF-8.');
        }
        return jsonEncode(descriptor);
      }, probe: (path) async => probedPath = path);

      final document = await loader.load(descriptorPath);
      final zone = document.placeholderZones.single;
      expect(zone.kind, 'vendor.example.binary-widget');
      expect(zone.reason, PlaceholderReason.unsupportedType);
      expect(zone.permittedActions, isNot(0));
      expect(probedPath, 'extension.agscene/resources/widget.bin');
    },
  );
}
