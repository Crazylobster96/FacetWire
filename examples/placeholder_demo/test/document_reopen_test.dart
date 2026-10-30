// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';
import 'package:facetwire_placeholder_demo/src/document_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pdfrx/pdfrx.dart';

Future<void> ready(WidgetTester tester) async {
  for (var i = 0; i < 200; i++) {
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 20)),
    );
    await tester.pump();
    if (find.byType(LinearProgressIndicator).evaluate().isEmpty &&
        find.byType(CircularProgressIndicator).evaluate().isEmpty) {
      await tester.pump();
      return;
    }
  }
  fail('Document did not become ready');
}

// Valid synthetic PDF whose uncompressed stream crosses pdfrx's 1 MiB
// in-memory threshold. xref offsets are computed, not repaired by the engine.
Uint8List largePdf(int marker) {
  final content =
      'BT /F1 20 Tf 20 150 Td (Large document $marker) Tj ET\n'
      '${List.filled(2200, '%${'x' * 1000}\n').join()}';
  final objects = [
    '<< /Type /Catalog /Pages 2 0 R >>',
    '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>',
    '<< /Length ${content.length} >>\nstream\n${content}endstream',
    '<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>',
  ];
  final out = StringBuffer('%PDF-1.4\n');
  final offsets = <int>[];
  for (var i = 0; i < objects.length; i++) {
    offsets.add(out.length);
    out.write('${i + 1} 0 obj\n${objects[i]}\nendobj\n');
  }
  final xref = out.length;
  out.write('xref\n0 6\n0000000000 65535 f \n');
  for (final offset in offsets) {
    out.write('${offset.toString().padLeft(10, '0')} 00000 n \n');
  }
  out.write('trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n$xref\n%%EOF\n');
  return Uint8List.fromList(ascii.encode(out.toString()));
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  testWidgets(
    'real host reopens DOCX and PDF repeatedly after page navigation',
    (tester) async {
      Pdfrx.cacheDirectoryPath = Directory.systemTemp.path;
      await tester.binding.setSurfaceSize(const Size(1200, 1000));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      await tester.pumpWidget(const MaterialApp(home: DocumentDemoScreen()));
      ImportedDocument? previous;
      for (var round = 0; round < 12; round++) {
        final extension = round % 3 == 2 ? 'docx' : 'pdf';
        await tester.tap(find.byKey(ValueKey('document-sample-$extension')));
        await ready(tester);
        expect(
          find.byKey(const ValueKey('document-open-error')),
          findsNothing,
          reason: 'round $round',
        );
        final current = tester
            .widget<DocumentPreview>(find.byType(DocumentPreview))
            .document;
        expect(current, isNot(same(previous)), reason: 'round $round');
        expect(current.format.name, extension);
        final counter = tester
            .widget<Text>(find.byKey(const ValueKey('document-page-count')))
            .data!;
        expect(
          counter,
          startsWith('1 / '),
          reason: 'round $round: $extension $counter',
        );
        final count = int.parse(counter.split(' / ').last);
        expect(count, greaterThanOrEqualTo(2));
        if (extension == 'pdf') {
          expect(
            tester.widget<RawImage>(find.byType(RawImage)).image,
            isNotNull,
          );
        }
        await tester.tap(find.byKey(const ValueKey('document-next-page')));
        await ready(tester);
        expect(find.text('2 / $count'), findsOneWidget);
        expect(tester.takeException(), isNull, reason: 'round $round');
        previous = current;
      }
      await tester.pumpWidget(const SizedBox());
      await tester.runAsync(() => previous!.close());
    },
  );

  testWidgets('large PDF sessions survive overlapping replacement and close', (
    tester,
  ) async {
    Pdfrx.cacheDirectoryPath = Directory.systemTemp.path;
    await tester.runAsync(() async {
      ImportedDocument? previous;
      try {
        for (var i = 0; i < 8; i++) {
          final current = await const PdfFormatPlugin()
              .open(largePdf(i), 'large-$i.pdf', const DocumentLimits())
              .timeout(const Duration(seconds: 15));
          final rendering = current.fixedPages!.render(0, 400);
          await previous?.close();
          previous = current;
          final raster = await rendering.timeout(const Duration(seconds: 15));
          expect(raster.bytes.any((b) => b != 0), isTrue);
        }
      } finally {
        await previous?.close();
      }
    });
    expect(tester.takeException(), isNull);
  });
  testWidgets(
    'rapid imports before first page paint leave latest document visible',
    (tester) async {
      Pdfrx.cacheDirectoryPath = Directory.systemTemp.path;
      await tester.binding.setSurfaceSize(const Size(1200, 1000));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      await tester.pumpWidget(const MaterialApp(home: DocumentDemoScreen()));
      for (var i = 0; i < 16; i++) {
        final extension = i % 3 == 1 ? 'docx' : 'pdf';
        await tester.tap(find.byKey(ValueKey('document-sample-$extension')));
        // Wait only for import, deliberately not for page raster/decoding.
        for (var spin = 0; spin < 100; spin++) {
          await tester.runAsync(
            () => Future<void>.delayed(const Duration(milliseconds: 2)),
          );
          await tester.pump();
          final button = tester.widget<TextButton>(
            find.byKey(const ValueKey('document-sample-pdf')),
          );
          if (button.onPressed != null) break;
        }
        expect(find.byKey(const ValueKey('document-open-error')), findsNothing);
        expect(
          tester
              .widget<DocumentPreview>(find.byType(DocumentPreview))
              .document
              .format
              .name,
          extension,
        );
        expect(tester.takeException(), isNull);
      }
      await ready(tester);
      expect(tester.widget<RawImage>(find.byType(RawImage)).image, isNotNull);
      expect(find.text('1 / 2'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.runAsync(
        () => Future<void>.delayed(const Duration(milliseconds: 50)),
      );
      expect(tester.takeException(), isNull);
    },
  );
}
