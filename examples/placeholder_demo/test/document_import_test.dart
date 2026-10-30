// SPDX-License-Identifier: MPL-2.0
import 'dart:io';
import 'dart:ui' as ui;

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';
import 'package:facetwire_placeholder_demo/src/document_demo.dart';
import 'package:facetwire_placeholder_demo/src/document_flow_client.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pdfrx/pdfrx.dart';

Future<void> capture(WidgetTester tester, String name) async {
  if (Platform.environment['FACETWIRE_DOCUMENT_QA'] != '1') return;
  final boundary = tester.renderObject<RenderRepaintBoundary>(
    find.byKey(const ValueKey('document-qa')),
  );
  await tester.runAsync(() async {
    final image = await boundary.toImage();
    try {
      final png = await image.toByteData(format: ui.ImageByteFormat.png);
      final file = File('build/document-import-qa/$name.png');
      await file.parent.create(recursive: true);
      await file.writeAsBytes(png!.buffer.asUint8List());
    } finally {
      image.dispose();
    }
  });
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUpAll(() async {
    if (Platform.environment['FACETWIRE_DOCUMENT_QA'] != '1') return;
    final fontPath = Platform.environment['FACETWIRE_QA_FONT'];
    if (fontPath == null) {
      throw StateError('Set FACETWIRE_QA_FONT for visual QA.');
    }
    final font = FontLoader('Arial')
      ..addFont(File(fontPath).readAsBytes().then(ByteData.sublistView));
    await font.load();
  });
  testWidgets('real DOCX imports and paginates through native Flow', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final doc = (await tester.runAsync(
      () async => const DocxFormatPlugin().open(
        await File('assets/document-import/sample.docx').readAsBytes(),
        'sample.docx',
        const DocumentLimits(),
      ),
    ))!;
    addTearDown(doc.close);
    final blocks = prepareDocumentBlocks(doc);
    final plan = await composeImportedDocument(
      blocks,
      doc.pageSize,
      doc.margin,
    );
    expect(plan.backend, 'Native Flow');
    expect(plan.pageCount, greaterThanOrEqualTo(2));
    expect(plan.fragments.length, blocks.length);
    for (final f in plan.fragments) {
      expect(
        f.bounds.bottom,
        lessThanOrEqualTo(doc.pageSize.height - doc.margin + 0.1),
      );
    }
    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(fontFamily: 'Arial'),
        home: Scaffold(
          body: RepaintBoundary(
            key: const ValueKey('document-qa'),
            child: Material(
              color: const Color(0xffedf0f4),
              child: DocumentPreview(
                document: doc,
                composeFlow: composeImportedDocument,
                pageBackground: Colors.white,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
    await tester.runAsync(() async {
      for (final image in tester.widgetList<Image>(find.byType(Image))) {
        await precacheImage(
          image.image,
          tester.element(find.byType(DocumentPreview)),
        );
      }
    });
    await tester.pumpAndSettle();
    expect(find.textContaining('Native Flow'), findsOneWidget);
    await capture(tester, 'docx-page-1');
    await tester.tap(find.byKey(const ValueKey('document-next-page')));
    await tester.pumpAndSettle();
    expect(find.text('2 / ${plan.pageCount}'), findsOneWidget);
    await capture(tester, 'docx-page-2');
    expect(tester.takeException(), isNull);
  });
  testWidgets('real PDFium renders transparent pages and closes safely', (
    tester,
  ) async {
    // flutter test has no registered path_provider. Supply its cache directory;
    // PDF parsing/rendering below still uses the real PDFium DLL.
    Pdfrx.cacheDirectoryPath = Directory.systemTemp.path;
    final document = await tester.runAsync(() async {
      final doc = await const PdfFormatPlugin()
          .open(
            await File('assets/document-import/sample.pdf').readAsBytes(),
            'sample.pdf',
            const DocumentLimits(),
          )
          .timeout(const Duration(seconds: 20));
      expect(doc.fixedPages!.sizes.length, 2);
      for (var page = 0; page < 2; page++) {
        final raster = await doc.fixedPages!.render(page, 600);
        expect(raster.width * raster.height, lessThanOrEqualTo(4000000));
        expect(raster.bytes.length, raster.width * raster.height * 4);
        expect(raster.bytes[3], 0); // No forced opaque page background.
        expect(raster.bytes.where((b) => b != 0).length, greaterThan(5000));
      }
      return doc;
    });
    expect(document, isNotNull);
    await tester.binding.setSurfaceSize(const Size(1200, 1000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(fontFamily: 'Arial'),
        home: Scaffold(
          body: RepaintBoundary(
            key: const ValueKey('document-qa'),
            child: Material(
              color: const Color(0xffedf0f4),
              child: DocumentPreview(
                document: document!,
                composeFlow: composeImportedDocument,
                pageBackground: Colors.white,
              ),
            ),
          ),
        ),
      ),
    );
    for (var page = 0; page < 2; page++) {
      for (var attempt = 0; attempt < 60; attempt++) {
        await tester.runAsync(
          () => Future<void>.delayed(const Duration(milliseconds: 50)),
        );
        await tester.pump();
        if (find.byType(CircularProgressIndicator).evaluate().isEmpty) break;
      }
      await tester.pumpAndSettle();
      expect(tester.widget<RawImage>(find.byType(RawImage)).image, isNotNull);
      expect(find.text('${page + 1} / 2'), findsOneWidget);
      await capture(tester, 'pdf-backend-page-${page + 1}');
      if (page == 0) {
        await tester.tap(find.byKey(const ValueKey('document-next-page')));
      }
    }
    await tester.pumpWidget(const SizedBox());
    await tester.runAsync(() async {
      await document.close();
      await document.close();
      await expectLater(document.fixedPages!.render(0, 300), throwsStateError);
      await expectLater(
        const PdfFormatPlugin().open(
          await File('assets/document-import/sample.pdf').readAsBytes(),
          'too-many-pages.pdf',
          const DocumentLimits(maxPages: 1),
        ),
        throwsA(
          isA<DocumentImportException>().having(
            (e) => e.code,
            'code',
            'page_limit',
          ),
        ),
      );
    });
  });
  testWidgets('bad input reports error and leaves previous document visible', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.pumpWidget(const MaterialApp(home: DocumentDemoScreen()));
    await tester.tap(find.byKey(const ValueKey('document-sample-docx')));
    for (var attempt = 0; attempt < 40; attempt++) {
      await tester.runAsync(
        () => Future<void>.delayed(const Duration(milliseconds: 50)),
      );
      await tester.pump();
      if (find.byType(DocumentPreview).evaluate().isNotEmpty) break;
    }
    await tester.pumpAndSettle();
    expect(find.byType(DocumentPreview), findsOneWidget);
    final original = tester
        .widget<DocumentPreview>(find.byType(DocumentPreview))
        .document;
    await tester.enterText(
      find.byKey(const ValueKey('document-file-path')),
      'does-not-exist.docx',
    );
    await tester.tap(find.text('打开'));
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 200)),
    );
    await tester.pumpAndSettle();
    expect(find.byKey(const ValueKey('document-open-error')), findsOneWidget);
    expect(
      tester.widget<DocumentPreview>(find.byType(DocumentPreview)).document,
      same(original),
    );
    expect(tester.takeException(), isNull);
  });
  testWidgets(
    'oversized paragraphs are sliced without dropping text geometry',
    (tester) async {
      final paragraph = DocumentParagraph(
        const DocumentSource('digest', 'part', '/p'),
        [DocumentRun(List.filled(200, 'Wrapped line.').join('\n'))],
      );
      final doc = ImportedDocument(
        name: 'long.docx',
        sourceHash: 'digest',
        format: DocumentFormat.docx,
        pluginId: 'test',
        layout: DocumentLayout.flow,
        blocks: [paragraph],
      );
      final blocks = prepareDocumentBlocks(doc);
      expect(blocks.length, greaterThan(1));
      expect(
        blocks.every((b) => b.block.source.id == paragraph.source.id),
        isTrue,
      );
      expect(
        blocks.fold<double>(0, (h, b) => h + b.height),
        closeTo(blocks.first.fullHeight, 0.01),
      );
      final plan = await composeImportedDocument(
        blocks,
        doc.pageSize,
        doc.margin,
      );
      expect(plan.fragments.length, blocks.length);
    },
  );
  testWidgets('document entry and preview adapt to phone viewport', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(390, 844));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.pumpWidget(const MaterialApp(home: DocumentDemoScreen()));
    expect(tester.takeException(), isNull);
    final doc = ImportedDocument(
      name: 'small.docx',
      sourceHash: 'digest',
      format: DocumentFormat.docx,
      pluginId: 'test',
      layout: DocumentLayout.flow,
      blocks: [
        DocumentParagraph(const DocumentSource('digest', 'part', '/p'), const [
          DocumentRun('Phone preview'),
        ]),
      ],
    );
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: DocumentPreview(
            document: doc,
            composeFlow: composeImportedDocument,
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(tester.takeException(), isNull);
    expect(find.text('1 / 1'), findsOneWidget);
  });
}
