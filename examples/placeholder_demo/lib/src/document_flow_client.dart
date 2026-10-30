// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';
import 'dart:ui';

import 'package:ffi/ffi.dart';
import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';

const _asset =
    'package:facetwire_placeholder_demo/facetwire_placeholder_demo_bridge.dart';

final class _Block extends Struct {
  @Float()
  external double width;
  @Float()
  external double height;
  @Uint32()
  external int breakBefore;
  @Uint32()
  external int keepNext;
}

final class _Buffer extends Struct {
  external Pointer<Uint8> data;
  @Uint64()
  external int length;
}

@Native<Int32 Function(Pointer<Pointer<Void>>)>(
  symbol: 'fwui_context_create',
  assetId: _asset,
)
external int _create(Pointer<Pointer<Void>> result);
@Native<Void Function(Pointer<Void>)>(
  symbol: 'fwui_context_destroy',
  assetId: _asset,
)
external void _destroy(Pointer<Void> context);
@Native<Void Function(Pointer<_Buffer>)>(
  symbol: 'fwui_buffer_release',
  assetId: _asset,
)
external void _release(Pointer<_Buffer> buffer);
@Native<
  Int32 Function(
    Pointer<Void>,
    Float,
    Float,
    Float,
    Pointer<_Block>,
    Uint32,
    Pointer<_Buffer>,
  )
>(symbol: 'fwui_compose_document_blocks', assetId: _asset)
external int _compose(
  Pointer<Void> context,
  double width,
  double height,
  double margin,
  Pointer<_Block> blocks,
  int count,
  Pointer<_Buffer> output,
);

Future<DocumentFlowPlan> composeImportedDocument(
  List<PreparedDocumentBlock> blocks,
  DocumentPageSize page,
  double margin,
) async {
  if (blocks.isEmpty || blocks.length > 10000) {
    throw StateError('Invalid block count');
  }
  final handle = calloc<Pointer<Void>>();
  final input = calloc<_Block>(blocks.length);
  final output = calloc<_Buffer>();
  try {
    if (_create(handle) != 0 || handle.value == nullptr) {
      throw StateError('Native Flow initialization failed');
    }
    for (var i = 0; i < blocks.length; i++) {
      input[i]
        ..width = blocks[i].width
        ..height = blocks[i].height
        ..breakBefore = blocks[i].breakBefore ? 1 : 0
        ..keepNext = blocks[i].keepNext ? 1 : 0;
    }
    final status = _compose(
      handle.value,
      page.width,
      page.height,
      margin,
      input,
      blocks.length,
      output,
    );
    if (status != 0 || output.ref.data == nullptr) {
      throw StateError('Native document Flow status $status');
    }
    final json = jsonDecode(
      utf8.decode(output.ref.data.asTypedList(output.ref.length)),
    ) as Map<String, dynamic>;
    if (json['status'] != 0 ||
        json['complete'] != true ||
        json['balanced'] != true) {
      throw StateError('Native document Flow incomplete: ${json['status']}');
    }
    return DocumentFlowPlan(
      json['pageCount'] as int,
      [
        for (final f
            in (json['fragments'] as List).cast<Map<String, dynamic>>())
          DocumentFlowFragment(
            f['index'] as int,
            f['page'] as int,
            Rect.fromLTWH(
              (f['x'] as num).toDouble(),
              (f['y'] as num).toDouble(),
              (f['width'] as num).toDouble(),
              (f['height'] as num).toDouble(),
            ),
          ),
      ],
      backend: 'Native Flow',
      diagnosticFlags: json['diagnostics'] as int,
    );
  } finally {
    if (handle.value != nullptr) _destroy(handle.value);
    _release(output);
    calloc.free(output);
    calloc.free(input);
    calloc.free(handle);
  }
}
