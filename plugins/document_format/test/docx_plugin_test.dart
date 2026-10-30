// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_format/src/bounded_zip.dart';
import 'package:test/test.dart';

Uint8List fixture(
  String body, {
  String type = 'application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml',
  Map<String, String> extra = const {},
  String prefix = 'w',
  Map<String, Uint8List> binaryExtra = const {},
}) {
  final files = <String, String>{
    '[Content_Types].xml':
        '<Types><Override PartName="/word/document.xml" ContentType="$type"/></Types>',
    '_rels/.rels': '<Relationships><Relationship Id="main" Type="http://x/officeDocument" Target="word/document.xml"/></Relationships>',
    'word/document.xml':
        '<$prefix:document xmlns:$prefix="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><$prefix:body>${body.replaceAll('w:', '$prefix:')}</$prefix:body></$prefix:document>',
    ...extra,
  };
  final archive = Archive();
  for (final entry in files.entries) {
    final bytes = utf8.encode(entry.value);
    archive.addFile(ArchiveFile(entry.key, bytes.length, bytes));
  }
  for (final entry in binaryExtra.entries) {
    archive.addFile(ArchiveFile(entry.key, entry.value.length, entry.value));
  }
  return Uint8List.fromList(ZipEncoder().encode(archive));
}

// Header-only resources exercise importer metadata accounting, not decoding.
Uint8List imageHeader(int width, int height) {
  final bytes = Uint8List(24)..setAll(0, [137, 80, 78, 71, 13, 10, 26, 10]);
  ByteData.sublistView(bytes)
    ..setUint32(16, width)
    ..setUint32(20, height);
  return bytes;
}

Uint8List imageFixture(List<String> references, Map<String, Uint8List> images) {
  return fixture(
    references
            .map(
              (id) =>
                  '<w:p><w:r><w:drawing>'
                  '<a:blip xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
                  'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" r:embed="$id"/>'
                  '</w:drawing></w:r></w:p>',
            )
            .join() +
        '<w:p><w:r><w:t>Text after images</w:t></w:r></w:p>',
    extra: {
      'word/_rels/document.xml.rels':
          '<Relationships>' +
          images.keys
              .map(
                (id) =>
                    '<Relationship Id="$id" Type="http://x/image" Target="media/$id.png"/>',
              )
              .join() +
          '</Relationships>',
    },
    binaryExtra: {
      for (final entry in images.entries)
        'word/media/${entry.key}.png': entry.value,
    },
  );
}

final registry = DocumentPluginRegistry([const DocxFormatPlugin()]);
void main() {
  test(
    'image assets own immutable bytes and preserve independent placements',
    () {
      final bytes = imageHeader(100, 100);
      final asset = DocumentImageAsset(bytes);
      bytes[0] = 0;
      expect(asset.bytes[0], 137);
      expect(() => asset.bytes[0] = 0, throwsUnsupportedError);
      expect(
        () => DocumentImageAsset(bytes, maxDecodePixels: 0),
        throwsArgumentError,
      );
      final first = DocumentImage.fromAsset(
        const DocumentSource('h', 'p', '/a'),
        asset,
        width: 100,
        height: 80,
      );
      final second = DocumentImage.fromAsset(
        const DocumentSource('h', 'p', '/b'),
        asset,
        width: 200,
        height: 160,
      );
      expect(identical(first.bytes, second.bytes), isTrue);
      expect(first.source.id, isNot(second.source.id));
      expect(first.width, isNot(second.width));
    },
  );
  test(
    'identical image bytes in different parts count once even at budget',
    () async {
      final doc = await registry.open(
        imageFixture(
          ['a', 'b', 'a'],
          {'a': imageHeader(100, 100), 'b': imageHeader(100, 100)},
        ),
        'aliases.docx',
        limits: const DocumentLimits(maxImageResourceBytes: 24),
      );
      final images = doc.blocks.whereType<DocumentImage>().toList();
      expect(images, hasLength(3));
      expect(identical(images[0].bytes, images[1].bytes), isTrue);
      expect(images.map((i) => i.source.id).toSet(), hasLength(3));
    },
  );
  test('unique resource limit degrades images, preserves text and resets on reopen', () async {
    final bytes = imageFixture(
      ['a', 'b', 'a', 'b'],
      {'a': imageHeader(100, 100), 'b': imageHeader(200, 100)},
    );
    for (var attempt = 0; attempt < 3; attempt++) {
      final doc = await registry.open(
        bytes,
        'budget.docx',
        limits: const DocumentLimits(maxImageResourceBytes: 24),
      );
      expect(doc.blocks.whereType<DocumentImage>(), hasLength(2));
      expect(doc.blocks.whereType<DocumentPlaceholder>(), hasLength(2));
      expect((doc.blocks.last as DocumentParagraph).text, 'Text after images');
      expect(
        doc.diagnostics.where((d) => d.code == 'image_unavailable'),
        hasLength(2),
      );
      expect(
        doc.blocks.whereType<DocumentPlaceholder>().first.reason,
        contains('48'),
      );
    }
  });
  test(
    'oversized source remains a local placeholder and decode limit reaches IR',
    () async {
      final doc = await registry.open(
        imageFixture(
          ['huge', 'valid'],
          {'huge': imageHeader(5000, 5000), 'valid': imageHeader(4000, 3000)},
        ),
        'source-limit.docx',
        limits: const DocumentLimits(maxImageDecodePixels: 250000),
      );
      expect(
        doc.blocks.whereType<DocumentPlaceholder>().single.reason,
        contains('25000000'),
      );
      expect(
        doc.blocks.whereType<DocumentImage>().single.asset.maxDecodePixels,
        250000,
      );
      expect((doc.blocks.last as DocumentParagraph).text, 'Text after images');
    },
  );
  test(
    'repeated image references do not reject the document at 32 MP',
    () async {
      final bytes = imageFixture(
        ['photo', 'photo', 'photo'],
        {'photo': imageHeader(4000, 3000)},
      );
      for (var attempt = 0; attempt < 3; attempt++) {
        final doc = await registry.open(bytes, 'repeated.docx');
        final images = doc.blocks.whereType<DocumentImage>().toList();
        expect(images, hasLength(3));
        expect(identical(images[0].bytes, images[1].bytes), isTrue);
        expect(doc.blocks.last, isA<DocumentParagraph>());
      }
    },
  );
  test('distinct photos above 32 MP aggregate remain importable', () async {
    final doc = await registry.open(
      imageFixture(
        ['a', 'b', 'c'],
        {
          'a': imageHeader(4000, 3000),
          'b': imageHeader(4000, 3001),
          'c': imageHeader(4000, 3002),
        },
      ),
      'photos.docx',
    );
    expect(doc.blocks.whereType<DocumentImage>(), hasLength(3));
  });
  test(
    'real DOCX preserves text image table page break and deterministic source',
    () async {
      final bytes = await File(
        '../../examples/placeholder_demo/assets/document-import/sample.docx',
      ).readAsBytes();
      final first = await registry.open(bytes, 'sample.docx');
      final second = await registry.open(bytes, 'sample.docx');
      expect(first.blocks.whereType<DocumentImage>(), isNotEmpty);
      expect(first.blocks.whereType<DocumentTable>(), hasLength(1));
      final paragraphs = first.blocks.whereType<DocumentParagraph>().toList();
      expect(paragraphs.any((p) => p.text.contains('中文段落')), isTrue);
      expect(
        paragraphs.any((p) => p.breakBefore && p.text.contains('Second page')),
        isTrue,
      );
      expect(paragraphs.last.text, contains('Paragraph 12'));
      expect(
        first.blocks.map((b) => b.source.id),
        second.blocks.map((b) => b.source.id),
      );
      expect(
        first.blocks.map((b) => b.source.id).toSet().length,
        first.blocks.length,
      );
    },
  );
  test('prefix independent, style runs, explicit page break', () async {
    final bytes = fixture(
      '<w:p><w:r><w:rPr><w:b/><w:sz w:val="36"/></w:rPr><w:t>Hello</w:t><w:br w:type="page"/><w:t>World</w:t></w:r></w:p>',
      prefix: 'alternate',
    );
    final doc = await registry.open(bytes, 'test.docx');
    final p = doc.blocks.cast<DocumentParagraph>();
    expect(p.first.text, 'Hello');
    expect(p.last.text, 'World');
    expect(p.last.breakBefore, isTrue);
    expect(p.first.runs.first.bold, isTrue);
    expect(p.first.runs.first.size, 24);
  });
  test(
    'external resources produce diagnostics without network fetch',
    () async {
      final doc = await registry.open(
        fixture(
          '<w:p><w:r><w:t>Safe</w:t></w:r></w:p>',
          extra: {
            'word/_rels/document.xml.rels': '<Relationships><Relationship Id="r1" Type="http://x/image" Target="https://example.invalid/image.png" TargetMode="External"/></Relationships>',
          },
        ),
        'external.docx',
      );
      expect(doc.diagnostics.any((d) => d.code == 'external_resource'), isTrue);
    },
  );
  test('unsupported body objects are visible placeholders', () async {
    final doc = await registry.open(fixture('<w:altChunk/>'), 'x.docx');
    expect(doc.blocks.single, isA<DocumentPlaceholder>());
  });
  test('streaming inflate enforces actual size, not only ZIP declarations', () {
    final archive = Archive()
      ..addFile(ArchiveFile.string('payload', 'a' * 100000));
    final bytes = Uint8List.fromList(ZipEncoder().encode(archive));
    final data = ByteData.sublistView(bytes);
    final directory = data.getUint32(bytes.length - 6, Endian.little);
    data.setUint32(directory + 24, 1, Endian.little);
    expect(
      () => BoundedZip(bytes, const DocumentLimits()).read('payload'),
      throwsA(
        isA<DocumentImportException>().having(
          (e) => e.code,
          'code',
          'inflate_limit',
        ),
      ),
    );
  });
  test('rejects wrong package type and legacy extensions', () async {
    await expectLater(
      registry.open(
        fixture('', type: 'application/vnd.ms-excel.sheet.main+xml'),
        'x.docx',
      ),
      throwsA(isA<DocumentImportException>()),
    );
    for (final name in ['x.doc', 'x.docm', 'x.xlsx', 'x.pptx']) {
      await expectLater(
        registry.open(fixture(''), name),
        throwsA(isA<DocumentImportException>()),
      );
    }
  });
  test('rejects traversal DTD corrupt and input/archive budgets', () async {
    for (final data in [
      Uint8List.fromList([1, 2, 3]),
      fixture('', extra: {'../escape': 'bad'}),
      fixture(
        '',
        extra: {'word/document.xml': '<!DOCTYPE d [<!ENTITY x "bad">]><d/>'},
      ),
    ]) {
      await expectLater(
        registry.open(data, 'bad.docx'),
        throwsA(isA<DocumentImportException>()),
      );
    }
    await expectLater(
      registry.open(
        fixture(''),
        'x.docx',
        limits: const DocumentLimits(maxInputBytes: 4),
      ),
      throwsA(isA<DocumentImportException>()),
    );
    await expectLater(
      registry.open(
        fixture(''),
        'x.docx',
        limits: const DocumentLimits(maxEntries: 1),
      ),
      throwsA(isA<DocumentImportException>()),
    );
    await expectLater(
      registry.open(
        fixture('<w:p><w:r><w:t>${'a' * 10000}</w:t></w:r></w:p>'),
        'x.docx',
        limits: const DocumentLimits(maxPartBytes: 1000),
      ),
      throwsA(isA<DocumentImportException>()),
    );
  });
}
