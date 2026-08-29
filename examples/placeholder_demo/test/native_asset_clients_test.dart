// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/flow_runtime_client.dart';
import 'package:facetwire_placeholder_demo/src/native_asset_demo_client.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('one Native Asset serves Placeholder and Flow contracts', () async {
    final placeholder = NativeAssetDemoClient.open();
    final flow = NativeAssetRuntimeClient.open();
    try {
      final snapshot = await placeholder.snapshot();
      final report = jsonDecode(
        await flow.composeFlowDemo(
          width: 600,
          height: 700,
          contentCase: 0,
          virtualPages: false,
        ),
      ) as Map<String, Object?>;

      expect(
        snapshot['pluginId'],
        'org.facetwire.reference.placeholder-renderer',
      );
      expect(report['capability'], 'facetwire.layout.flow');
      expect(report['nativeRuntime'], isTrue);
      expect(report['fragmentCount'], 3);
    } finally {
      await placeholder.close();
      await flow.close();
    }
  });
}
