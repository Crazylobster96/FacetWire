// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:io';
import 'dart:typed_data' show BytesBuilder;

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';
import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'document_flow_client.dart';

class DocumentDemoScreen extends StatefulWidget {
  const DocumentDemoScreen({
    super.key,
    this.registry,
    this.composeFlow = composeImportedDocument,
  });
  final DocumentPluginRegistry? registry;
  final DocumentFlowComposer composeFlow;
  @override
  State<DocumentDemoScreen> createState() => _DocumentDemoScreenState();
}

class _DocumentDemoScreenState extends State<DocumentDemoScreen> {
  late final _registry =
      widget.registry ??
      DocumentPluginRegistry([
        const DocxFormatPlugin(),
        const PdfFormatPlugin(),
      ]);
  final _path = TextEditingController();
  ImportedDocument? _document;
  Object? _error;
  var _loading = false, _paper = true;
  var _request = 0;
  static const _limits = DocumentLimits();
  Future<void> _open(Future<Uint8List> Function() load, String name) async {
    final token = ++_request;
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      final bytes = await load();
      final next = await _registry.open(bytes, name, limits: _limits);
      if (!mounted || token != _request) {
        await next.close();
        return;
      }
      final previous = _document;
      setState(() {
        _document = next;
        _loading = false;
      });
      await previous?.close();
    } catch (error) {
      if (mounted && token == _request) {
        setState(() {
          _error = error;
          _loading = false;
        });
      }
    }
  }

  Future<Uint8List> _readFile(String path) async {
    final file = File(path);
    // Read at most limit+1 even if the file changes after stat.
    final handle = await file.open();
    try {
      final size = await handle.length();
      if (size > _limits.maxInputBytes) {
        throw const DocumentImportException('input_limit', '文件超过 32 MiB。');
      }
      return await handle.read(_limits.maxInputBytes + 1);
    } finally {
      await handle.close();
    }
  }

  Future<void> _choose() async {
    try {
      final file = await openFile(
        acceptedTypeGroups: const [
          XTypeGroup(
            label: 'DOCX / PDF',
            extensions: ['docx', 'pdf'],
            uniformTypeIdentifiers: [
              'org.openxmlformats.wordprocessingml.document',
              'com.adobe.pdf',
            ],
          ),
        ],
      );
      if (file == null || !mounted) return;
      _path.text = file.path;
      await _open(() async {
        if (await file.length() > _limits.maxInputBytes) {
          throw const DocumentImportException('input_limit', '文件超过 32 MiB。');
        }
        if (file.path.isNotEmpty && File(file.path).isAbsolute) {
          return _readFile(file.path);
        }
        // A provider URI may not be a filesystem path. Bound its stream too,
        // since a length check alone cannot protect against a changing file.
        final output = BytesBuilder(copy: false);
        await for (final chunk in file.openRead()) {
          if (output.length + chunk.length > _limits.maxInputBytes) {
            throw const DocumentImportException('input_limit', '文件超过 32 MiB。');
          }
          output.add(chunk);
        }
        return output.takeBytes();
      }, file.name);
    } catch (e) {
      if (mounted) setState(() => _error = e);
    }
  }

  void _sample(String extension) => _open(() async {
    final data = await rootBundle.load(
      'assets/document-import/sample.$extension',
    );
    return data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes);
  }, 'sample.$extension');
  @override
  void dispose() {
    ++_request;
    _path.dispose();
    unawaited(_document?.close());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Documents / 文档导入 0.1')),
    body: Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            children: [
              Row(
                children: [
                  Expanded(
                    child: TextField(
                      controller: _path,
                      key: const ValueKey('document-file-path'),
                      decoration: const InputDecoration(
                        labelText: '本地 DOCX / PDF 路径',
                        border: OutlineInputBorder(),
                      ),
                      onSubmitted: _loading
                          ? null
                          : (_) => _open(
                              () => _readFile(_path.text.trim()),
                              _path.text.trim(),
                            ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  FilledButton(
                    onPressed: _loading
                        ? null
                        : () => _open(
                            () => _readFile(_path.text.trim()),
                            _path.text.trim(),
                          ),
                    child: const Text('打开'),
                  ),
                ],
              ),
              Wrap(
                spacing: 8,
                crossAxisAlignment: WrapCrossAlignment.center,
                children: [
                  OutlinedButton.icon(
                    onPressed: _loading ? null : _choose,
                    icon: const Icon(Icons.folder_open),
                    label: const Text('选择文件'),
                  ),
                  TextButton(
                    key: const ValueKey('document-sample-docx'),
                    onPressed: _loading ? null : () => _sample('docx'),
                    child: const Text('DOCX 示例'),
                  ),
                  TextButton(
                    key: const ValueKey('document-sample-pdf'),
                    onPressed: _loading ? null : () => _sample('pdf'),
                    child: const Text('PDF 示例'),
                  ),
                  Checkbox(
                    value: _paper,
                    onChanged: (v) => setState(() => _paper = v!),
                  ),
                  const Text('Demo 白色纸张背景'),
                ],
              ),
            ],
          ),
        ),
        if (_loading) const LinearProgressIndicator(),
        if (_error != null)
          Padding(
            padding: const EdgeInsets.all(8),
            child: SelectableText(
              '打开失败（原文档未修改）：$_error',
              key: const ValueKey('document-open-error'),
            ),
          ),
        if (_document != null)
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            child: Text(
              '${_document!.name} · ${_document!.pluginId} · source ${_document!.sourceHash.substring(0, 12)}',
            ),
          ),
        Expanded(
          child: _document == null
              ? const Center(
                  child: Text(
                    '选择本地 DOCX / PDF，或打开示例。\n只读导入；XLSX、PPTX 和旧版 Office 尚未支持。',
                    textAlign: TextAlign.center,
                  ),
                )
              : DocumentPreview(
                  key: ObjectKey(_document),
                  document: _document!,
                  composeFlow: widget.composeFlow,
                  pageBackground: _paper ? Colors.white : Colors.transparent,
                ),
        ),
      ],
    ),
  );
}
