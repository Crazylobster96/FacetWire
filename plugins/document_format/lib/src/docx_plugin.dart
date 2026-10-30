// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:xml/xml.dart';

import 'bounded_zip.dart';
import 'model.dart';

final class DocxFormatPlugin implements DocumentFormatPlugin {
  const DocxFormatPlugin();
  @override
  String get id => 'facetwire.format.docx.reference.v0.1';
  @override
  DocumentFormat get format => DocumentFormat.docx;
  @override
  Future<ImportedDocument> open(
    Uint8List bytes,
    String name,
    DocumentLimits limits,
  ) => Isolate.run(() => _DocxReader(bytes, name, limits).read());
}

final class _DocxReader {
  _DocxReader(this.bytes, this.name, this.limits);
  final Uint8List bytes;
  final String name;
  final DocumentLimits limits;
  late final zip = BoundedZip(bytes, limits);
  late final hash = documentDigest(bytes);
  String part = 'word/document.xml';
  final diagnostics = <DocumentDiagnostic>[];
  final blocks = <DocumentBlock>[];
  final styles = <String, XmlElement>{};
  final relationships = <String, String>{};
  final relationTypes = <String, String>{};
  int _imageBytes = 0;
  final _imageParts = <String, DocumentImageAsset>{};
  final _imageContents = <String, DocumentImageAsset>{};
  final _imageFailures = <String, String>{};
  static const _w = {
    'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
    'http://purl.oclc.org/ooxml/wordprocessingml/main',
  };
  bool _is(XmlElement e, String local) =>
      e.name.local == local && _w.contains(e.namespaceUri);
  XmlElement? _child(XmlElement? e, String name) =>
      e?.childElements.where((c) => _is(c, name)).firstOrNull;
  String? _attr(XmlElement? e, String name) =>
      e?.attributes.where((a) => a.name.local == name).firstOrNull?.value;
  Iterable<XmlElement> _desc(XmlElement e, String name) =>
      e.descendantElements.where((c) => _is(c, name));
  DocumentSource _source(String location) =>
      DocumentSource(hash, part, location);
  void _add(DocumentBlock block) {
    if (blocks.length >= limits.maxNodes)
      throw const DocumentImportException('node_limit', '文档节点超过预算。');
    blocks.add(block);
  }

  void _warn(String code, String message, [String? location]) {
    if (diagnostics.length < 200 &&
        !diagnostics.any((d) => d.code == code && d.location == location)) {
      diagnostics.add(DocumentDiagnostic(code, message, location: location));
    }
  }

  XmlElement? _xml(String path) {
    final data = zip.read(path);
    if (data == null) return null;
    final text = utf8.decode(data);
    if (RegExp(r'<!\s*(DOCTYPE|ENTITY)', caseSensitive: false).hasMatch(text)) {
      throw const DocumentImportException('unsafe_xml', '禁止 DTD 与实体声明。');
    }
    final root = XmlDocument.parse(text).rootElement;
    final stack = <(XmlElement, int)>[(root, 0)];
    var count = 0;
    while (stack.isNotEmpty) {
      final (element, depth) = stack.removeLast();
      if (++count > limits.maxNodes * 20 || depth > 128) {
        throw const DocumentImportException('xml_limit', 'XML 节点或嵌套超过预算。');
      }
      stack.addAll(element.childElements.map((c) => (c, depth + 1)));
    }
    return root;
  }

  String _resolve(String owner, String target) {
    if (target.contains('\\') ||
        target.contains('%') ||
        target.contains(':') ||
        target.startsWith('/') ||
        target.contains('\x00')) {
      throw const DocumentImportException('unsafe_relationship', '包关系目标非法。');
    }
    final components = owner.split('/')..removeLast();
    for (final segment in target.split('/')) {
      if (segment == '.' || segment.isEmpty) continue;
      if (segment == '..') {
        if (components.isEmpty)
          throw const DocumentImportException(
            'unsafe_relationship',
            '包关系越过根目录。',
          );
        components.removeLast();
      } else {
        components.add(segment);
      }
    }
    return components.join('/');
  }

  String _relsPath(String path) {
    final bits = path.split('/');
    final file = bits.removeLast();
    return [...bits, '_rels', '$file.rels'].join('/');
  }

  ImportedDocument read() {
    try {
      if (limits.maxImageDecodePixels <= 0) {
        throw const DocumentImportException('invalid_limits', '图片解码预算必须大于零。');
      }
      final rootRels = _xml('_rels/.rels');
      if (rootRels == null)
        throw const DocumentImportException('not_docx', '缺少 OPC 包关系。');
      final main = rootRels.childElements
          .where((e) => (_attr(e, 'Type') ?? '').endsWith('/officeDocument'))
          .firstOrNull;
      if (main == null || _attr(main, 'TargetMode') == 'External') {
        throw const DocumentImportException('not_docx', '缺少内部主文档。');
      }
      part = _resolve('', _attr(main, 'Target') ?? '');
      final types = _xml('[Content_Types].xml');
      final contentType = types?.childElements
          .where((e) => _attr(e, 'PartName') == '/$part')
          .firstOrNull;
      if (_attr(contentType, 'ContentType') !=
          'application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml') {
        throw const DocumentImportException(
          'not_docx',
          '不是普通 DOCX 主文档（宏/模板/其他 Office 格式不支持）。',
        );
      }
      final rels = _xml(_relsPath(part));
      if (rels != null) {
        final seen = <String>{};
        for (final r in rels.childElements) {
          final id = _attr(r, 'Id');
          if (id == null) continue;
          if (!seen.add(id))
            throw const DocumentImportException(
              'duplicate_relationship',
              '重复关系 ID。',
            );
          if (_attr(r, 'TargetMode') == 'External') {
            _warn('external_resource', '外部链接/资源未加载。');
            continue;
          }
          relationships[id] = _resolve(part, _attr(r, 'Target') ?? '');
          relationTypes[id] = _attr(r, 'Type') ?? '';
        }
      }
      final styleId = relationTypes.keys
          .where((id) => relationTypes[id]!.endsWith('/styles'))
          .firstOrNull;
      final styleRoot = styleId == null ? null : _xml(relationships[styleId]!);
      if (styleRoot != null) {
        for (final s in styleRoot.childElements.where((s) => _is(s, 'style'))) {
          styles[_attr(s, 'styleId') ?? ''] = s;
        }
      }
      final root = _xml(part);
      if (root == null || !_is(root, 'document'))
        throw const DocumentImportException('not_docx', '正文 XML 无效。');
      final body = _child(root, 'body');
      if (body == null)
        throw const DocumentImportException('not_docx', '正文缺失。');
      var index = 0;
      for (final node in body.childElements) {
        final location = '/body/${node.name.local}[${index++}]';
        if (_is(node, 'p')) {
          _paragraph(node, location);
        } else if (_is(node, 'tbl')) {
          _table(node, location);
        } else if (!_is(node, 'sectPr')) {
          _add(
            DocumentPlaceholder(
              _source(location),
              '未支持正文元素：${node.name.local}',
            ),
          );
          _warn('unsupported_element', '该正文元素显示为占位。', location);
        }
        if (blocks.length > limits.maxNodes)
          throw const DocumentImportException('node_limit', '文档节点超过预算。');
      }
      final section = _child(body, 'sectPr');
      final size = _child(section, 'pgSz');
      final width = _number(_attr(size, 'w'), 11910) / 15;
      final height = _number(_attr(size, 'h'), 16845) / 15;
      final margins = _child(section, 'pgMar');
      final margin = (_number(_attr(margins, 'left'), 960) / 15).clamp(
        24.0,
        144.0,
      );
      if (width < 200 ||
          height < 200 ||
          width > 4000 ||
          height > 4000 ||
          2 * margin >= width) {
        throw const DocumentImportException('page_size', '页面尺寸超出首版范围。');
      }
      if (_desc(body, 'sectPr').length > 1)
        _warn('mixed_sections', '多节页面属性简化为末节纸张；不保证 Word 原始页码。');
      if (section != null &&
          section.childElements.any(
            (e) => [
              'headerReference',
              'footerReference',
              'cols',
            ].contains(e.name.local),
          )) {
        _warn('section_features', '页眉、页脚、多栏节属性尚未完整导入。');
      }
      _warn('reflow_preview', '结构化重排预览；字体替代、对称页边距、简化段落样式与块分页可能改变版面。');
      return ImportedDocument(
        name: name,
        sourceHash: hash,
        format: DocumentFormat.docx,
        pluginId: const DocxFormatPlugin().id,
        layout: DocumentLayout.flow,
        blocks: blocks,
        diagnostics: diagnostics,
        pageSize: DocumentPageSize(width, height),
        margin: margin,
      );
    } on DocumentImportException {
      rethrow;
    } catch (e) {
      throw DocumentImportException('invalid_docx', 'DOCX 解析失败：$e');
    }
  }

  double _number(String? text, double fallback) {
    final n = double.tryParse(text ?? '');
    return n != null && n.isFinite ? n : fallback;
  }

  List<XmlElement> _styleChain(String? id) {
    final chain = <XmlElement>[];
    final seen = <String>{};
    while (id != null &&
        styles[id] != null &&
        chain.length < 16 &&
        seen.add(id)) {
      final style = styles[id]!;
      chain.insert(0, style);
      id = _attr(_child(style, 'basedOn'), 'val');
    }
    return chain;
  }

  bool _flag(XmlElement? p, String key, bool fallback) {
    final element = _child(p, key);
    if (element == null) return fallback;
    return !['0', 'false', 'off'].contains(_attr(element, 'val'));
  }

  DocumentRun _run(
    String text,
    Iterable<XmlElement?> props, {
    int heading = 0,
  }) {
    var bold = heading > 0, italic = false, underline = false;
    var size = heading == 1 ? 28.0 : (heading > 1 ? 22.0 : 16.0);
    var color = 0xff303844;
    for (final p in props) {
      bold = _flag(p, 'b', bold);
      italic = _flag(p, 'i', italic);
      final u = _child(p, 'u');
      if (u != null)
        underline = !['none', '0', 'false'].contains(_attr(u, 'val'));
      final sz = _attr(_child(p, 'sz'), 'val');
      if (sz != null) size = (_number(sz, 24) * 2 / 3).clamp(6.0, 96.0);
      final value = _attr(_child(p, 'color'), 'val');
      if (value != null && RegExp(r'^[a-fA-F0-9]{6}$').hasMatch(value))
        color = 0xff000000 | int.parse(value, radix: 16);
    }
    return DocumentRun(
      text,
      bold: bold,
      italic: italic,
      underline: underline,
      size: size,
      color: color,
    );
  }

  void _paragraph(XmlElement p, String location) {
    final pPr = _child(p, 'pPr');
    final styleName = _attr(_child(pPr, 'pStyle'), 'val');
    final chain = _styleChain(styleName);
    final heading =
        int.tryParse(
          RegExp(
                r'heading\s*(\d)',
                caseSensitive: false,
              ).firstMatch(styleName ?? '')?.group(1) ??
              '',
        ) ??
        0;
    final paragraphProps = [...chain.map((s) => _child(s, 'pPr')), pPr];
    var alignment = 'left', pageBreak = false, keepNext = heading > 0;
    for (final prop in paragraphProps) {
      alignment = _attr(_child(prop, 'jc'), 'val') ?? alignment;
      pageBreak = _flag(prop, 'pageBreakBefore', pageBreak);
      keepNext = _flag(prop, 'keepNext', keepNext);
    }
    var runs = <DocumentRun>[];
    var segment = 0;
    void flush({bool force = false}) {
      if (runs.isEmpty && !force) return;
      _add(
        DocumentParagraph(
          _source('$location/segment[${segment++}]'),
          runs,
          heading: heading,
          alignment: alignment,
          breakBefore: pageBreak,
          keepNext: keepNext,
        ),
      );
      runs = [];
      pageBreak = false;
    }

    if (_child(pPr, 'numPr') != null) {
      runs.add(_run('- ', const []));
      _warn('list_marker', '列表编号简化为短横线；保留条目正文。');
    }
    if (_desc(p, 'del').isNotEmpty || _desc(p, 'ins').isNotEmpty)
      _warn('tracked_changes', '按最终正文预览：包含插入内容，不展示删除内容。');
    for (final r in _desc(p, 'r')) {
      // Nested text boxes contain their own paragraphs. Do not duplicate their
      // runs in the outer paragraph; the drawing is an explicit placeholder.
      if (!identical(
        r.ancestors
            .whereType<XmlElement>()
            .where((a) => _is(a, 'p'))
            .firstOrNull,
        p,
      ))
        continue;
      if (r.ancestors.whereType<XmlElement>().any((a) => _is(a, 'del')))
        continue;
      final rPr = _child(r, 'rPr');
      if (_flag(rPr, 'vanish', false)) {
        _warn('hidden_text', '隐藏文字未展示。');
        continue;
      }
      final props = [
        ...chain.map((s) => _child(s, 'rPr')),
        ..._styleChain(_attr(_child(rPr, 'rStyle'), 'val'))
            .map((s) => _child(s, 'rPr')),
        rPr,
      ];
      for (final c in r.childElements) {
        if (_is(c, 't')) {
          runs.add(_run(c.innerText, props, heading: heading));
        } else if (_is(c, 'tab')) {
          runs.add(_run('    ', props, heading: heading));
        } else if (_is(c, 'br') || _is(c, 'cr')) {
          if (_attr(c, 'type') == 'page') {
            flush(force: true);
            pageBreak = true;
          } else {
            runs.add(_run('\n', props, heading: heading));
          }
        } else if (_is(c, 'drawing')) {
          flush();
          _image(c, '$location/image[${segment++}]');
        } else if (!_is(c, 'rPr') && !_is(c, 'lastRenderedPageBreak')) {
          _warn('run_feature', '字段、公式、旧式图形等内容尚未完整支持。');
          if (!['fldChar', 'instrText'].contains(c.name.local))
            runs.add(_run('[${c.name.local}]', props));
        }
      }
    }
    if (_desc(p, 'oMath').isNotEmpty ||
        p.descendantElements.any((c) => c.name.local == 'oMath')) {
      runs.add(_run('[公式未支持]', const []));
      _warn('math', '公式未导入。');
    }
    flush(force: runs.isNotEmpty || segment == 0 || pageBreak);
  }

  void _image(XmlElement drawing, String location) {
    final blip = drawing.descendantElements
        .where((e) => e.name.local == 'blip')
        .firstOrNull;
    final resource = relationships[_attr(blip, 'embed')];
    final extent = drawing.descendantElements
        .where((e) => e.name.local == 'extent')
        .firstOrNull;
    final alt = drawing.descendantElements
        .where((e) => e.name.local == 'docPr')
        .firstOrNull;
    final asset = resource == null ? null : _imageAsset(resource);
    if (asset == null) {
      final reason = _imageFailures[resource] ?? '图片缺失或格式未支持。';
      _add(DocumentPlaceholder(_source(location), reason));
      _warn('image_unavailable', reason, location);
      return;
    }
    final width = (_number(_attr(extent, 'cx'), 2857500) / 9525).clamp(
      1.0,
      2000.0,
    );
    final height = (_number(_attr(extent, 'cy'), 1905000) / 9525).clamp(
      1.0,
      2000.0,
    );
    _add(
      DocumentImage.fromAsset(
        _source(location),
        asset,
        width: width,
        height: height,
        alt: _attr(alt, 'descr') ?? _attr(alt, 'name') ?? '',
      ),
    );
    _warn('image_as_block', '嵌入/浮动图片按独立块展示，不复刻 Word 环绕锚点。');
  }

  DocumentImageAsset? _imageAsset(String resource) {
    if (_imageParts.containsKey(resource)) return _imageParts[resource];
    if (_imageFailures.containsKey(resource)) return null;
    // ZIP/CRC/inflate safety failures still reject the package, never swallowed.
    final data = zip.read(resource);
    if (data == null) {
      _imageFailures[resource] = '图片资源缺失：$resource';
      return null;
    }
    final (width, height) = _imageSize(data);
    if (width <= 0 || height <= 0) {
      _imageFailures[resource] = '图片格式或尺寸无效（支持 PNG/JPEG/GIF）：$resource';
      return null;
    }
    final pixels = width * height;
    if (pixels > limits.maxImageSourcePixels) {
      _imageFailures[resource] =
          '图片原始像素 $pixels 超过单图预算 ${limits.maxImageSourcePixels}：$resource';
      return null;
    }
    final digest = documentDigest(data);
    var asset = _imageContents[digest];
    if (asset == null) {
      if (_imageBytes + data.length > limits.maxImageResourceBytes) {
        _imageFailures[resource] =
            '图片资源字节 ${_imageBytes + data.length} 超过去重预算 ${limits.maxImageResourceBytes}：$resource';
        return null;
      }
      asset = DocumentImageAsset(
        data,
        maxDecodePixels: limits.maxImageDecodePixels,
      );
      _imageBytes += data.length;
      _imageContents[digest] = asset;
    }
    _imageParts[resource] = asset;
    return asset;
  }

  (int, int) _imageSize(Uint8List b) {
    int w = 0, h = 0;
    if (b.length >= 24 &&
        b[0] == 137 &&
        b[1] == 80 &&
        b[2] == 78 &&
        b[3] == 71) {
      final d = ByteData.sublistView(b);
      w = d.getUint32(16);
      h = d.getUint32(20);
    } else if (b.length >= 10 &&
        ascii.decode(b.sublist(0, 3), allowInvalid: true) == 'GIF') {
      final d = ByteData.sublistView(b);
      w = d.getUint16(6, Endian.little);
      h = d.getUint16(8, Endian.little);
    } else if (b.length >= 4 && b[0] == 255 && b[1] == 216) {
      var p = 2;
      while (p + 8 < b.length) {
        if (b[p++] != 255) break;
        while (p < b.length && b[p] == 255) {
          p++;
        }
        if (p + 3 >= b.length) break;
        final marker = b[p++];
        if (marker == 217 || marker == 218) break;
        if (marker == 1 || (marker >= 208 && marker <= 215)) continue;
        final len = b[p] * 256 + b[p + 1];
        if (len < 2 || p + len > b.length) break;
        if ([192, 193, 194].contains(marker)) {
          if (len < 8) break;
          h = b[p + 3] * 256 + b[p + 4];
          w = b[p + 5] * 256 + b[p + 6];
          break;
        }
        p += len;
      }
    }
    return (w, h);
  }

  void _table(XmlElement table, String location) {
    final rows = <List<String>>[];
    for (final row in table.childElements.where((e) => _is(e, 'tr'))) {
      final cells = <String>[];
      for (final cell in row.childElements.where((e) => _is(e, 'tc'))) {
        cells.add(
          _desc(
            cell,
            'p',
          ).map((p) => _desc(p, 't').map((t) => t.innerText).join()).join('\n'),
        );
      }
      if (cells.length > 32 || rows.length >= 200)
        throw const DocumentImportException('table_limit', '表格超过 200 行或 32 列。');
      if (cells.isNotEmpty) rows.add(cells);
    }
    _add(DocumentTable(_source(location), rows));
    _warn('basic_table', '表格按单元格文本预览，均分列宽；合并、内嵌对象及复杂样式未保留。');
  }
}
