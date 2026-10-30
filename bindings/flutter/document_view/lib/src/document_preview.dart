// SPDX-License-Identifier: MPL-2.0
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:flutter/material.dart';

import 'flow_blocks.dart';

/// A read-only page surface. The host owns and closes [document].
class DocumentPreview extends StatefulWidget {
  const DocumentPreview({
    super.key,
    required this.document,
    required this.composeFlow,
    this.pageBackground = Colors.transparent,
  });
  final ImportedDocument document;
  final DocumentFlowComposer composeFlow;
  final Color pageBackground;
  @override
  State<DocumentPreview> createState() => _DocumentPreviewState();
}

class _DocumentPreviewState extends State<DocumentPreview> {
  int _page = 0, _token = 0;
  double _zoom = 1, _opacity = 1;
  bool _loading = true;
  Object? _error;
  ui.Image? _image;
  DocumentFlowPlan? _plan;
  List<PreparedDocumentBlock> _blocks = const [];
  int get _count =>
      widget.document.fixedPages?.sizes.length ?? _plan?.pageCount ?? 1;
  DocumentPageSize get _size =>
      widget.document.fixedPages?.sizes[_page] ?? widget.document.pageSize;
  @override
  void initState() {
    super.initState();
    _initialize();
  }

  @override
  void didUpdateWidget(DocumentPreview oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.document != widget.document) {
      _page = 0;
      _plan = null;
      _blocks = [];
      _initialize();
    }
  }

  Future<void> _initialize() async {
    final token = ++_token;
    setState(() {
      _loading = true;
      _error = null;
      _image?.dispose();
      _image = null;
    });
    try {
      if (widget.document.layout == DocumentLayout.flow) {
        final blocks = prepareDocumentBlocks(widget.document);
        final plan = blocks.isEmpty
            ? DocumentFlowPlan(1, const [], backend: 'Empty document')
            : await widget.composeFlow(
                blocks,
                widget.document.pageSize,
                widget.document.margin,
              );
        if (!mounted || token != _token) return;
        setState(() {
          _blocks = blocks;
          _plan = plan;
          _loading = false;
        });
      } else {
        await _loadPdfPage(token);
      }
    } catch (e) {
      if (mounted && token == _token)
        setState(() {
          _error = e;
          _loading = false;
        });
    }
  }

  Future<void> _loadPdfPage(int token) async {
    final raster = await widget.document.fixedPages!.render(_page, 1500);
    if (!mounted || token != _token) return;
    final buffer = await ui.ImmutableBuffer.fromUint8List(raster.bytes);
    ui.ImageDescriptor? descriptor;
    ui.Image? image;
    try {
      descriptor = ui.ImageDescriptor.raw(
        buffer,
        width: raster.width,
        height: raster.height,
        pixelFormat: raster.bgra
            ? ui.PixelFormat.bgra8888
            : ui.PixelFormat.rgba8888,
      );
      final codec = await descriptor.instantiateCodec();
      try {
        image = (await codec.getNextFrame()).image;
      } finally {
        codec.dispose();
      }
    } finally {
      descriptor?.dispose();
      buffer.dispose();
    }
    if (!mounted || token != _token) {
      image.dispose();
      return;
    }
    setState(() {
      _image?.dispose();
      _image = image;
      _loading = false;
    });
  }

  void _changePage(int value) {
    if (value < 0 || value >= _count || _loading) return;
    setState(() => _page = value);
    if (widget.document.fixedPages != null) _initialize();
  }

  @override
  void dispose() {
    ++_token;
    _image?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Column(
    children: [
      Padding(
        padding: const EdgeInsets.all(8),
        child: Wrap(
          crossAxisAlignment: WrapCrossAlignment.center,
          spacing: 8,
          children: [
            Text(
              '${widget.document.format.name.toUpperCase()} · ${widget.document.layout == DocumentLayout.fixed ? 'PDFium fixed pages' : _plan?.backend ?? 'Flow loading'}',
            ),
            IconButton(
              key: const ValueKey('document-previous-page'),
              tooltip: '上一页',
              onPressed: _page > 0 && !_loading
                  ? () => _changePage(_page - 1)
                  : null,
              icon: const Icon(Icons.chevron_left),
            ),
            Text(
              '${_page + 1} / $_count',
              key: const ValueKey('document-page-count'),
            ),
            IconButton(
              key: const ValueKey('document-next-page'),
              tooltip: '下一页',
              onPressed: _page + 1 < _count && !_loading
                  ? () => _changePage(_page + 1)
                  : null,
              icon: const Icon(Icons.chevron_right),
            ),
            Text('缩放 ${(_zoom * 100).round()}%'),
            SizedBox(
              width: 120,
              child: Slider(
                value: _zoom,
                min: 0.5,
                max: 3,
                onChanged: (v) => setState(() => _zoom = v),
              ),
            ),
            Text('透明度 ${(_opacity * 100).round()}%'),
            SizedBox(
              width: 120,
              child: Slider(
                key: const ValueKey('document-opacity'),
                value: _opacity,
                onChanged: (v) => setState(() => _opacity = v),
              ),
            ),
          ],
        ),
      ),
      if (_plan?.diagnosticFlags != null && _plan!.diagnosticFlags != 0)
        Text('Flow 约束已放宽：${_plan!.diagnosticFlags}，请检查分页。'),
      Expanded(
        child: _loading
            ? const Center(child: CircularProgressIndicator())
            : _error != null
            ? Center(child: SelectableText('文档显示失败：$_error'))
            : LayoutBuilder(
                builder: (context, constraints) {
                  final size = _size;
                  final scale =
                      math.max(
                        0.05,
                        math.min(
                          (constraints.maxWidth - 24) / size.width,
                          (constraints.maxHeight - 24) / size.height,
                        ),
                      ) *
                      _zoom;
                  return SingleChildScrollView(
                    child: Center(
                      child: SingleChildScrollView(
                        scrollDirection: Axis.horizontal,
                        child: Padding(
                          padding: const EdgeInsets.all(12),
                          child: SizedBox(
                            width: size.width * scale,
                            height: size.height * scale,
                            child: FittedBox(
                              child: SizedBox(
                                width: size.width,
                                height: size.height,
                                child: Opacity(
                                  opacity: _opacity,
                                  child: ColoredBox(
                                    color: widget.pageBackground,
                                    child: ClipRect(
                                      child:
                                          widget.document.layout ==
                                              DocumentLayout.fixed
                                          ? RawImage(
                                              image: _image,
                                              fit: BoxFit.fill,
                                            )
                                          : Stack(
                                              clipBehavior: Clip.hardEdge,
                                              children: [
                                                for (final fragment
                                                    in _plan!.fragments.where(
                                                      (f) => f.page == _page,
                                                    ))
                                                  Positioned.fromRect(
                                                    rect: fragment.bounds,
                                                    child: Tooltip(
                                                      message:
                                                          _blocks[fragment
                                                                  .index]
                                                              .block
                                                              .source
                                                              .id,
                                                      child: DocumentBlockView(
                                                        displayScale: scale,
                                                        item:
                                                            _blocks[fragment
                                                                .index],
                                                      ),
                                                    ),
                                                  ),
                                              ],
                                            ),
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ),
                      ),
                    ),
                  );
                },
              ),
      ),
      if (widget.document.diagnostics.isNotEmpty)
        ExpansionTile(
          title: Text('导入诊断 (${widget.document.diagnostics.length}) · 只读预览'),
          children: [
            SizedBox(
              height: 140,
              child: ListView(
                children: [
                  for (final d in widget.document.diagnostics)
                    ListTile(
                      dense: true,
                      title: Text(d.message),
                      subtitle: Text(d.code),
                    ),
                ],
              ),
            ),
          ],
        ),
    ],
  );
}
