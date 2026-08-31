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
          pageMode: 0,
        ),
      ) as Map<String, Object?>;
      final columns = jsonDecode(
        await flow.composeFlowDemo(
          width: 600,
          height: 700,
          contentCase: 2,
          pageMode: 2,
        ),
      ) as Map<String, Object?>;
      final inline = jsonDecode(
        await flow.composeFlowDemo(
          width: 600,
          height: 700,
          contentCase: 3,
          pageMode: 0,
        ),
      ) as Map<String, Object?>;
      final floatStart = jsonDecode(
        await flow.composeFlowDemo(
          width: 600,
          height: 700,
          contentCase: 6,
          pageMode: 0,
        ),
      ) as Map<String, Object?>;

      expect(
        snapshot['pluginId'],
        'org.facetwire.reference.placeholder-renderer',
      );
      expect(report['capability'], 'facetwire.layout.flow');
      expect(report['nativeRuntime'], isTrue);
      expect(report['fragmentCount'], 3);
      expect(columns['nativeRuntime'], isTrue);
      expect(columns['pageMode'], 2);
      expect(columns['pageCount'], 1);
      expect(columns['columnCount'], 2);
      final columnFragments = columns['fragments']! as List<Object?>;
      expect((columnFragments.last! as Map<String, Object?>)['columnIndex'], 1);
      expect(inline['nativeRuntime'], isTrue);
      expect(inline['inlineObjects'], isTrue);
      expect(inline['fragmentCount'], 3);
      final inlineFragments = inline['fragments']! as List<Object?>;
      expect(
        (inlineFragments[1]! as Map<String, Object?>)['sourceItemId'],
        'image.inline.level-1',
      );
      expect((inlineFragments.last! as Map<String, Object?>)['textStart'], 7);
      expect(floatStart['placementMode'], 'float-start');
      expect(floatStart['inlineObjects'], isFalse);
      expect(floatStart['fragmentCount'], 3);
    } finally {
      await placeholder.close();
      await flow.close();
    }
  });
}
