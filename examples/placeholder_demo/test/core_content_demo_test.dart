// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:facetwire_placeholder_demo/src/demo_app.dart';
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

  test('loads the three-level rich media showcase', () async {
    final document = await CoreContentPackageLoader().load(
      richMediaShowcaseDescriptor,
    );

    expect(document.documents.length, 3);
    expect((document.width, document.height), (1280, 820));
    expect(document.zones.length, 12);
    expect(document.zones.where((zone) => zone.type == 'text').length, 3);
    expect(document.zones.where((zone) => zone.type == 'image').length, 2);
    expect(
      document.zones.where((zone) => zone.type == 'animated-image').length,
      2,
    );
    expect(document.zones.where((zone) => zone.type == 'chart').length, 2);
    expect(document.zones.where((zone) => zone.type == 'video').length, 1);
    final video = document.zones.singleWhere((zone) => zone.type == 'video');
    expect(video.resourceAsset, endsWith('showcase-video.mp4'));
    expect(video.posterAsset, endsWith('showcase-video-poster.png'));
    expect(
      document.zones
          .where((zone) => zone.type == 'document')
          .every((zone) => zone.documentFit == 'none'),
      isTrue,
    );
  });
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

  test('loads rich media resources from an uncompressed folder', () async {
    final document = await CoreContentPackageLoader().loadPath(
      'assets/documents/rich-media-showcase.agscene',
    );
    final video = document.zones.singleWhere((zone) => zone.type == 'video');

    expect(document.documents.length, 3);
    expect(video.resourceAsset, startsWith('file:'));
    expect(video.posterAsset, startsWith('file:'));
    expect(File.fromUri(Uri.parse(video.resourceAsset!)).existsSync(), isTrue);
    expect(File.fromUri(Uri.parse(video.posterAsset!)).existsSync(), isTrue);
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
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.pumpWidget(const MaterialApp(home: CoreContentDemoScreen()));
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

    expect(find.text('FacetWire Rich Media Showcase'), findsOneWidget);
    expect(find.textContaining('3 recursive documents'), findsOneWidget);
    expect(activeSource, findsOneWidget);
    expect(find.byKey(const ValueKey('open-flow-layout-demo')), findsOneWidget);
    final canvasBox = find.byKey(const ValueKey('preview-canvas-box'));
    expect(tester.getSize(canvasBox).width, lessThan(1280));

    final actualSizeButton = find.text('固定 1:1');
    await tester.ensureVisible(actualSizeButton);
    await tester.tap(actualSizeButton);
    await tester.pumpAndSettle();
    expect(tester.getSize(canvasBox), const Size(1280, 820));
    expect(find.textContaining('根画布保持原始逻辑像素'), findsOneWidget);

    await tester.binding.setSurfaceSize(const Size(1400, 900));
    await tester.pumpAndSettle();
    expect(tester.getSize(canvasBox), const Size(1280, 820));
    await tester.binding.setSurfaceSize(const Size(1600, 1600));
    await tester.pumpAndSettle();

    for (final type in const [
      'text',
      'image',
      'animated-image',
      'chart',
      'video',
    ]) {
      expect(find.byKey(ValueKey('type-opacity:$type')), findsOneWidget);
    }
    final textOpacity = find.byKey(const ValueKey('type-opacity:text'));
    await tester.drag(textOpacity, const Offset(-180, 0));
    await tester.pump();
    expect(tester.widget<Slider>(textOpacity).value, lessThan(1));
    expect(
      find.byKey(const ValueKey('embedded-video:zone:video:level-1')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chart-bar:zone:chart:level-1:Text')),
      findsOneWidget,
    );

    for (final documentId in const [
      'document:rich-media-showcase:level-1',
      'document:rich-media-showcase:level-2',
      'document:rich-media-showcase:level-3',
    ]) {
      final canvas = tester.widget<DecoratedBox>(
        find.byKey(ValueKey('document-canvas:$documentId')),
      );
      expect((canvas.decoration as BoxDecoration).color, Colors.transparent);
    }

    final level2Zone = find.byKey(
      const ValueKey('$richMediaShowcaseDescriptor#zone:document:level-2'),
    );
    final level2Rect = tester.getRect(level2Zone);
    await tester.tapAt(Offset(level2Rect.left + 4, level2Rect.bottom - 4));
    await tester.pump();
    final selectedOpacity = find.byKey(
      const ValueKey('selected-layer-opacity'),
    );
    expect(tester.widget<Slider>(selectedOpacity).value, 1);
    await tester.drag(selectedOpacity, const Offset(-1000, 0));
    await tester.pump();
    expect(tester.widget<Slider>(selectedOpacity).value, 0);
    expect(
      tester
          .widget<Opacity>(
            find.byKey(
              const ValueKey('document-opacity:zone:document:level-2'),
            ),
          )
          .opacity,
      0,
    );
    expect(level2Zone, findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('open-demo-source')));
    await tester.pumpAndSettle();
    expect(find.byKey(const ValueKey('demo-source-path')), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('load-builtin-demo')));
    await tester.pump();
  });
}
