// SPDX-License-Identifier: MPL-2.0
import 'dart:typed_data';

import 'package:crypto/crypto.dart';

enum DocumentFormat { docx, pdf, xlsx, pptx }

enum DocumentLayout { flow, fixed }

final class DocumentLimits {
  const DocumentLimits({
    this.maxInputBytes = 32 * 1024 * 1024,
    this.maxExpandedBytes = 64 * 1024 * 1024,
    this.maxPartBytes = 8 * 1024 * 1024,
    this.maxEntries = 2048,
    this.maxNodes = 10000,
    this.maxPages = 500,
    this.maxPagePixels = 4000000,
    this.maxImageResourceBytes = 32 * 1024 * 1024,
    this.maxImageSourcePixels = 16000000,
    this.maxImageDecodePixels = 1000000,
  });
  final int maxInputBytes,
      maxExpandedBytes,
      maxPartBytes,
      maxEntries,
      maxNodes,
      maxPages,
      maxPagePixels,
      maxImageResourceBytes,
      maxImageSourcePixels,
      maxImageDecodePixels;
}

final class DocumentImportException implements Exception {
  const DocumentImportException(this.code, this.message);
  final String code, message;
  @override
  String toString() => '$code: $message';
}

final class DocumentDiagnostic {
  const DocumentDiagnostic(this.code, this.message, {this.location});
  final String code, message;
  final String? location;
}

final class DocumentSource {
  const DocumentSource(this.sha256, this.part, this.location);
  final String sha256, part, location;
  String get id => '$sha256:$part#$location';
}

final class DocumentRun {
  const DocumentRun(
    this.text, {
    this.bold = false,
    this.italic = false,
    this.underline = false,
    this.size = 16,
    this.color = 0xff303844,
  });
  final String text;
  final bool bold, italic, underline;
  final double size;
  final int color;
}

sealed class DocumentBlock {
  const DocumentBlock(this.source);
  final DocumentSource source;
}

final class DocumentParagraph extends DocumentBlock {
  DocumentParagraph(
    super.source,
    Iterable<DocumentRun> runs, {
    this.heading = 0,
    this.alignment = 'left',
    this.breakBefore = false,
    this.keepNext = false,
  }) : runs = List.unmodifiable(runs);
  final List<DocumentRun> runs;
  final int heading;
  final String alignment;
  final bool breakBefore, keepNext;
  String get text => runs.map((r) => r.text).join();
}

/// One immutable encoded resource shared by independently placed image blocks.
/// Hosts must bound retained decode dimensions, not sum source pixels per use.
final class DocumentImageAsset {
  DocumentImageAsset(Uint8List bytes, {this.maxDecodePixels = 1000000})
    : bytes = Uint8List.fromList(bytes).asUnmodifiableView() {
    if (maxDecodePixels <= 0) {
      throw ArgumentError.value(maxDecodePixels, 'maxDecodePixels');
    }
  }
  final Uint8List bytes;
  final int maxDecodePixels;
}

final class DocumentImage extends DocumentBlock {
  DocumentImage(
    super.source,
    Uint8List bytes, {
    required this.width,
    required this.height,
    this.alt = '',
  }) : asset = DocumentImageAsset(bytes);
  const DocumentImage.fromAsset(
    super.source,
    this.asset, {
    required this.width,
    required this.height,
    this.alt = '',
  });
  final DocumentImageAsset asset;
  Uint8List get bytes => asset.bytes;
  final double width, height;
  final String alt;
}

final class DocumentTable extends DocumentBlock {
  DocumentTable(super.source, Iterable<List<String>> rows)
    : rows = List.unmodifiable(rows.map((r) => List<String>.unmodifiable(r)));
  final List<List<String>> rows;
}

final class DocumentPlaceholder extends DocumentBlock {
  const DocumentPlaceholder(super.source, this.reason);
  final String reason;
}

final class DocumentPageSize {
  const DocumentPageSize(this.width, this.height);
  final double width, height;
}

/// Pixels are straight BGRA/RGBA, not a platform texture handle.
final class DocumentRaster {
  const DocumentRaster(this.bytes, this.width, this.height, {this.bgra = true});
  final Uint8List bytes;
  final int width, height;
  final bool bgra;
}

abstract interface class FixedDocumentPages {
  List<DocumentPageSize> get sizes;
  Future<DocumentRaster> render(int pageIndex, int targetWidth);
  Future<void> close();
}

final class ImportedDocument {
  ImportedDocument({
    required this.name,
    required this.sourceHash,
    required this.format,
    required this.pluginId,
    required this.layout,
    Iterable<DocumentBlock> blocks = const [],
    Iterable<DocumentDiagnostic> diagnostics = const [],
    this.fixedPages,
    this.pageSize = const DocumentPageSize(794, 1123),
    this.margin = 64,
  }) : blocks = List.unmodifiable(blocks),
       diagnostics = List.unmodifiable(diagnostics);
  final String name, sourceHash, pluginId;
  final DocumentFormat format;
  final DocumentLayout layout;
  final List<DocumentBlock> blocks;
  final List<DocumentDiagnostic> diagnostics;
  final DocumentPageSize pageSize;
  final double margin;
  final FixedDocumentPages? fixedPages;
  Future<void> close() async => fixedPages?.close();
}

abstract interface class DocumentFormatPlugin {
  String get id;
  DocumentFormat get format;
  Future<ImportedDocument> open(
    Uint8List bytes,
    String name,
    DocumentLimits limits,
  );
}

final class DocumentPluginRegistry {
  DocumentPluginRegistry(Iterable<DocumentFormatPlugin> plugins)
    : _plugins = {for (final plugin in plugins) plugin.format: plugin};
  final Map<DocumentFormat, DocumentFormatPlugin> _plugins;
  Future<ImportedDocument> open(
    Uint8List bytes,
    String name, {
    DocumentLimits limits = const DocumentLimits(),
  }) async {
    if (bytes.isEmpty || bytes.length > limits.maxInputBytes) {
      throw const DocumentImportException('input_limit', '文件为空或超过导入大小限制。');
    }
    final extension = name.split('.').last.toLowerCase();
    final format = switch (extension) {
      'docx' => DocumentFormat.docx,
      'pdf' => DocumentFormat.pdf,
      'xlsx' => DocumentFormat.xlsx,
      'pptx' => DocumentFormat.pptx,
      _ => null,
    };
    final plugin = _plugins[format];
    if (plugin == null) {
      throw const DocumentImportException(
        'unsupported_format',
        '首版仅支持 DOCX 和 PDF；XLSX/PPTX、旧版 Office 和宏文件尚未实现。',
      );
    }
    return plugin.open(bytes, name, limits);
  }
}

String documentDigest(Uint8List bytes) => sha256.convert(bytes).toString();
