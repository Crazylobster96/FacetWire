// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:facetwire_playground_ui_spike/src/core_content_demo.dart';
import 'package:facetwire_playground_ui_spike/src/spike_app.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test(
    'loads three native-size recursive documents and core content',
    () async {
      final document = await CoreContentPackageLoader().load(
        coreContentDemoDescriptor,
      );

      expect(document.documents.length, 3);
      expect(document.width, 960);
      expect(document.height, 640);
      expect(document.zones.length, 11);
      expect(document.zones.where((zone) => zone.type == 'text').length, 3);
      expect(document.zones.where((zone) => zone.type == 'image').length, 3);
      expect(
        document.zones.where((zone) => zone.type == 'animated-image').length,
        3,
      );

      final level2 = document.documents.elementAt(1);
      final level3 = document.documents.elementAt(2);
      expect((level2.width, level2.height), (500, 350));
      expect((level3.width, level3.height), (230, 170));
      expect(
        document.zones
            .where((zone) => zone.type == 'document')
            .every((zone) => zone.documentFit == 'none'),
        isTrue,
      );
    },
  );

  test('loads an uncompressed package by folder or descriptor path', () async {
    final loader = CoreContentPackageLoader();
    final folder = Directory(
      'assets/documents/core-content-overlap-demo.agscene',
    );
    final descriptor = File(
      '${folder.path}${Platform.pathSeparator}'
      'core-content-overlap-demo.agscene.dis.json',
    );

    final fromFolder = await loader.loadPath(folder.path);
    final fromDescriptor = await loader.loadPath(descriptor.path);

    expect(fromFolder.documents.length, 3);
    expect(fromDescriptor.zones.length, 11);
    expect(
      fromFolder.zones
          .where((zone) => zone.type == 'image')
          .every((zone) => zone.resourceAsset!.startsWith('file:')),
      isTrue,
    );
    expect(
      fromFolder.zones
          .where((zone) => zone.type == 'animated-image')
          .every((zone) => zone.resourceAsset!.startsWith('file:')),
      isTrue,
    );
  });

  test('parses explicit and positional demo paths', () {
    expect(parseDemoPathArguments(['--demo', r'D:\demo']), r'D:\demo');
    expect(parseDemoPathArguments(['--demo=/tmp/demo']), '/tmp/demo');
    expect(parseDemoPathArguments(['/tmp/demo']), '/tmp/demo');
    expect(parseDemoPathArguments(const []), isNull);
  });

  testWidgets('starts in demo and exposes independent opacity controls', (
    tester,
  ) async {
    await tester.pumpWidget(SpikeApp(client: DemoRuntimeClient()));
    await tester.pump();

    final activeSource = find.byKey(const ValueKey('active-demo-source'));
    for (
      var attempt = 0;
      attempt < 20 && activeSource.evaluate().isEmpty;
      attempt += 1
    ) {
      await tester.runAsync(
        () => Future<void>.delayed(const Duration(milliseconds: 20)),
      );
      await tester.pump(const Duration(milliseconds: 50));
    }

    expect(find.text('Text + Core Image 0.1 Demo'), findsOneWidget);
    expect(find.textContaining('3 recursive documents'), findsOneWidget);
    expect(activeSource, findsOneWidget);
    final canvasBox = find.byKey(const ValueKey('preview-canvas-box'));
    expect(tester.getSize(canvasBox).width, lessThan(960));

    final actualSizeButton = find.text('固定 1:1');
    await tester.ensureVisible(actualSizeButton);
    await tester.tap(actualSizeButton);
    await tester.pumpAndSettle();
    expect(tester.getSize(canvasBox), const Size(960, 640));
    expect(find.textContaining('根画布保持原始逻辑像素'), findsOneWidget);

    await tester.binding.setSurfaceSize(const Size(1400, 900));
    await tester.pumpAndSettle();
    expect(tester.getSize(canvasBox), const Size(960, 640));
    await tester.binding.setSurfaceSize(null);
    await tester.pumpAndSettle();

    final textOpacity = find.byKey(const ValueKey('type-opacity:text'));
    await tester.scrollUntilVisible(
      textOpacity,
      220,
      scrollable: find.descendant(
        of: find.byKey(const ValueKey('core-content-controls')),
        matching: find.byType(Scrollable),
      ).first,
    );
    expect(textOpacity, findsOneWidget);
    expect(find.byKey(const ValueKey('type-opacity:image')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('type-opacity:animated-image')),
      findsOneWidget,
    );
    await tester.drag(textOpacity, const Offset(-180, 0));
    await tester.pump();
    expect(tester.widget<Slider>(textOpacity).value, lessThan(1));

    await tester.tap(find.byKey(const ValueKey('open-demo-source')));
    await tester.pumpAndSettle();
    expect(find.byKey(const ValueKey('demo-source-path')), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('load-builtin-demo')));
    await tester.pump();
  });
}
