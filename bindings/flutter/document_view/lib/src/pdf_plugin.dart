// SPDX-License-Identifier: MPL-2.0
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:pdfrx/pdfrx.dart';

final class PdfFormatPlugin implements DocumentFormatPlugin {
  const PdfFormatPlugin();
  @override
  String get id => 'facetwire.format.pdf.pdfium.v0.1';
  @override
  DocumentFormat get format => DocumentFormat.pdf;
  @override
  Future<ImportedDocument> open(
    Uint8List bytes,
    String name,
    DocumentLimits limits,
  ) async {
    if (bytes.length < 5 ||
        bytes.length > limits.maxInputBytes ||
        String.fromCharCodes(bytes.take(5)) != '%PDF-') {
      throw const DocumentImportException('invalid_pdf', 'PDF 文件头无效或超出输入预算。');
    }
    PdfDocument? document;
    try {
      await pdfrxFlutterInitialize();
      final hash = documentDigest(bytes);
      document = await PdfDocument.openData(
        bytes,
        sourceName: hash,
        useProgressiveLoading: true,
      );
      if (document.pages.isEmpty || document.pages.length > limits.maxPages) {
        throw const DocumentImportException('page_limit', 'PDF 页数为空或超过预算。');
      }
      // Progressive open only loads the initial page. ensureLoaded() waits for
      // events; it does not start loading. Drive the bounded metadata pass first.
      await document.loadPagesProgressively();
      final sizes = <DocumentPageSize>[];
      for (final page in document.pages) {
        final loaded = await page.ensureLoaded();
        if (!loaded.width.isFinite ||
            !loaded.height.isFinite ||
            loaded.width <= 0 ||
            loaded.height <= 0 ||
            loaded.width > 20000 ||
            loaded.height > 20000 ||
            loaded.height / loaded.width > 100 ||
            loaded.width / loaded.height > 100) {
          throw const DocumentImportException('page_size', 'PDF 页面尺寸不在安全预览范围。');
        }
        sizes.add(DocumentPageSize(loaded.width, loaded.height));
      }
      return ImportedDocument(
        name: name,
        sourceHash: hash,
        format: format,
        pluginId: id,
        layout: DocumentLayout.fixed,
        fixedPages: _PdfPages(document, sizes, limits),
        diagnostics: const [
          DocumentDiagnostic(
            'fixed_page_preview',
            'PDF 固定页预览；不提供 OCR、元素编辑、密码输入或保存，不执行链接/脚本。',
          ),
        ],
      );
    } catch (e) {
      await document?.dispose();
      if (e is DocumentImportException) rethrow;
      throw DocumentImportException(
        'pdf_open_failed',
        'PDF 后端无法打开文件（可能损坏或需要密码）：$e',
      );
    }
  }
}

final class _PdfPages implements FixedDocumentPages {
  _PdfPages(this.document, List<DocumentPageSize> sizes, this.limits)
    : sizes = List.unmodifiable(sizes);
  final PdfDocument document;
  final DocumentLimits limits;
  @override
  final List<DocumentPageSize> sizes;
  bool _closed = false;
  Future<void> _tail = Future.value();
  Future<void>? _closeFuture;
  @override
  Future<DocumentRaster> render(int pageIndex, int targetWidth) {
    if (_closed) return Future.error(StateError('PDF session closed'));
    final operation = _tail.then((_) async {
      if (_closed) throw StateError('PDF session closed');
      RangeError.checkValidIndex(pageIndex, sizes);
      final size = sizes[pageIndex];
      final ratio = size.height / size.width;
      final width = math.max(
        1,
        math.min(
          targetWidth.clamp(1, 2400),
          math.sqrt(limits.maxPagePixels / ratio).floor() - 1,
        ),
      );
      final height = math.max(1, (width * ratio).floor());
      final bitmap = await document.pages[pageIndex].render(
        width: width,
        height: height,
        fullWidth: width.toDouble(),
        fullHeight: height.toDouble(),
        backgroundColor: 0x00000000,
        annotationRenderingMode: PdfAnnotationRenderingMode.annotationAndForms,
      );
      if (bitmap == null)
        throw const DocumentImportException('pdf_render_failed', 'PDF 页面渲染失败。');
      try {
        return DocumentRaster(
          Uint8List.fromList(bitmap.pixels),
          bitmap.width,
          bitmap.height,
        );
      } finally {
        bitmap.dispose();
      }
    });
    _tail = operation.then<void>((_) {}, onError: (Object _, StackTrace __) {});
    return operation;
  }

  @override
  Future<void> close() {
    _closed = true;
    return _closeFuture ??= _tail.then((_) => document.dispose());
  }
}
