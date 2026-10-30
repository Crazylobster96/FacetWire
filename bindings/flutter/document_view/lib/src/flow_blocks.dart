// SPDX-License-Identifier: MPL-2.0
import 'dart:math' as math;

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:flutter/material.dart';

final class PreparedDocumentBlock {
  const PreparedDocumentBlock(
    this.block,
    this.width,
    this.height, {
    this.offsetY = 0,
    this.fullHeight = 0,
    this.breakBefore = false,
    this.keepNext = false,
  });
  final DocumentBlock block;
  final double width, height, offsetY, fullHeight;
  final bool breakBefore, keepNext;
}

final class DocumentFlowFragment {
  const DocumentFlowFragment(this.index, this.page, this.bounds);
  final int index, page;
  final Rect bounds;
}

final class DocumentFlowPlan {
  DocumentFlowPlan(
    this.pageCount,
    Iterable<DocumentFlowFragment> fragments, {
    required this.backend,
    this.diagnosticFlags = 0,
  }) : fragments = List.unmodifiable(fragments);
  final int pageCount, diagnosticFlags;
  final String backend;
  final List<DocumentFlowFragment> fragments;
}

typedef DocumentFlowComposer = Future<DocumentFlowPlan> Function(
  List<PreparedDocumentBlock> blocks,
  DocumentPageSize size,
  double margin,
);

TextPainter _paragraphPainter(DocumentParagraph p, double width) => TextPainter(
  text: TextSpan(
    children: [
      for (final run in p.runs)
        TextSpan(
          text: run.text,
          style: TextStyle(
            fontFamily: 'Arial',
            fontSize: run.size,
            height: 1.35,
            fontWeight: run.bold ? FontWeight.w700 : FontWeight.w400,
            fontStyle: run.italic ? FontStyle.italic : FontStyle.normal,
            decoration: run.underline
                ? TextDecoration.underline
                : TextDecoration.none,
            color: Color(run.color),
          ),
        ),
    ],
  ),
  textDirection: TextDirection.ltr,
  textAlign: switch (p.alignment) {
    'center' => TextAlign.center,
    'right' || 'end' => TextAlign.right,
    'both' || 'justify' => TextAlign.justify,
    _ => TextAlign.left,
  },
)..layout(maxWidth: width);
TextPainter _cellPainter(String text, double width) => TextPainter(
  text: TextSpan(
    text: text,
    style: const TextStyle(
      fontFamily: 'Arial',
      color: Color(0xff303844),
      fontSize: 14,
      height: 1.35,
    ),
  ),
  textDirection: TextDirection.ltr,
)..layout(maxWidth: math.max(1, width));

List<double> _tableRowHeights(DocumentTable table, double width) {
  final columns = table.rows.fold<int>(1, (n, r) => math.max(n, r.length));
  final cellWidth = width / columns - 16;
  return [
    for (final row in table.rows)
      row.fold<double>(32, (h, text) {
        final painter = _cellPainter(text, cellWidth);
        final result = math.max(h, painter.height + 16);
        painter.dispose();
        return result;
      }),
  ];
}

List<PreparedDocumentBlock> prepareDocumentBlocks(ImportedDocument document) {
  final width = document.pageSize.width - 2 * document.margin;
  final maxHeight = document.pageSize.height - 2 * document.margin - 8;
  if (width < 120 || maxHeight < 40)
    throw const DocumentImportException('page_size', '正文区域过小。');
  final result = <PreparedDocumentBlock>[];
  for (final block in document.blocks) {
    var boundaries = <double>[];
    var height = 48.0;
    var blockWidth = width;
    var breakBefore = false, keepNext = false;
    if (block is DocumentParagraph) {
      final painter = _paragraphPainter(block, width);
      var y = 0.0;
      for (final line in painter.computeLineMetrics()) {
        y += line.height;
        boundaries.add(y);
      }
      height = math.max(22, painter.height);
      painter.dispose();
      breakBefore = block.breakBefore;
      keepNext = block.keepNext;
    } else if (block is DocumentTable) {
      var y = 0.0;
      for (final rowHeight in _tableRowHeights(block, width)) {
        y += rowHeight;
        boundaries.add(y);
      }
      height = math.max(32, y + 1);
    } else if (block is DocumentImage) {
      final scale = math.min(
        1.0,
        math.min(width / block.width, maxHeight / block.height),
      );
      height = block.height * scale;
      blockWidth = block.width * scale;
    }
    // Normal paragraphs/rows remain atomic. Oversized ones become visual slices
    // at line/row boundaries; source identity and full content are preserved.
    var offset = 0.0;
    while (height - offset > maxHeight) {
      if (result.length >= 10000)
        throw const DocumentImportException('layout_limit', '测量后的块数超过预算。');
      final end =
          boundaries
              .where((b) => b > offset && b <= offset + maxHeight)
              .lastOrNull ??
          (offset + maxHeight);
      result.add(
        PreparedDocumentBlock(
          block,
          blockWidth,
          end - offset,
          offsetY: offset,
          fullHeight: height,
          breakBefore: offset == 0 && breakBefore,
        ),
      );
      offset = end;
    }
    result.add(
      PreparedDocumentBlock(
        block,
        blockWidth,
        math.max(1, height - offset),
        offsetY: offset,
        fullHeight: height,
        breakBefore: offset == 0 && breakBefore,
        keepNext: keepNext,
      ),
    );
    if (result.length > 10000)
      throw const DocumentImportException('layout_limit', '测量后的块数超过预算。');
  }
  return result;
}

class DocumentBlockView extends StatelessWidget {
  const DocumentBlockView({
    super.key,
    required this.item,
    this.displayScale = 1,
  });
  final PreparedDocumentBlock item;
  final double displayScale;
  @override
  Widget build(BuildContext context) {
    final block = item.block;
    if (block is DocumentImage) {
      final ratio = MediaQuery.devicePixelRatioOf(context) * displayScale;
      var width = math.max(1, (item.width * ratio).ceil());
      var height = math.max(1, (item.height * ratio).ceil());
      final budget = block.asset.maxDecodePixels;
      if (width * height > budget) {
        final scale = math.sqrt(budget / (width * height));
        width = math.max(1, (width * scale).floor());
        height = math.max(1, (height * scale).floor());
        // Extremely narrow aspect ratios still obey the cap after rounding.
        width = math.min(width, budget);
        height = math.min(height, budget ~/ width);
      }
      return Image(
        image: ResizeImage(
          MemoryImage(block.bytes),
          width: width,
          height: height,
          policy: ResizeImagePolicy.fit,
          allowUpscaling: false,
        ),
        fit: BoxFit.contain,
        semanticLabel: block.alt,
        errorBuilder: (_, error, stack) => const Center(child: Text('图片解码失败')),
      );
    }
    return Semantics(
      label: block is DocumentParagraph ? block.text : block.source.location,
      child: ClipRect(
        child: CustomPaint(
          painter: _BlockPainter(item),
          size: Size(item.width, item.height),
        ),
      ),
    );
  }
}

class _BlockPainter extends CustomPainter {
  _BlockPainter(this.item);
  final PreparedDocumentBlock item;
  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.clipRect(Offset.zero & size);
    canvas.translate(0, -item.offsetY);
    final block = item.block;
    if (block is DocumentParagraph) {
      final painter = _paragraphPainter(block, item.width);
      painter.paint(canvas, Offset.zero);
      painter.dispose();
    } else if (block is DocumentTable) {
      final heights = _tableRowHeights(block, item.width);
      final columns = block.rows.fold<int>(
        1,
        (n, row) => math.max(n, row.length),
      );
      final w = item.width / columns;
      var y = 0.0;
      for (var r = 0; r < block.rows.length; r++) {
        for (var c = 0; c < columns; c++) {
          final rect = Rect.fromLTWH(c * w, y, w, heights[r]);
          if (r == 0)
            canvas.drawRect(rect, Paint()..color = const Color(0x14516d8a));
          canvas.drawRect(
            rect,
            Paint()
              ..color = const Color(0xffa7b3c2)
              ..style = PaintingStyle.stroke
              ..strokeWidth = 0.7,
          );
          final painter = _cellPainter(
            c < block.rows[r].length ? block.rows[r][c] : '',
            w - 16,
          );
          painter.paint(canvas, Offset(c * w + 8, y + 8));
          painter.dispose();
        }
        y += heights[r];
      }
    } else if (block is DocumentPlaceholder) {
      final painter = _cellPainter(block.reason, item.width - 16);
      painter.paint(canvas, const Offset(8, 8));
      painter.dispose();
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(_BlockPainter oldDelegate) => oldDelegate.item != item;
}
