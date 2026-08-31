// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

const _nativeAssetId =
    'package:facetwire_placeholder_demo/facetwire_placeholder_demo_bridge.dart';

final class _FlowNativeBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

typedef _CreateNative = Int32 Function(Pointer<Pointer<Void>>);
typedef _DestroyNative = Void Function(Pointer<Void>);
typedef _ComposeNative = Int32 Function(
  Pointer<Void>,
  Float,
  Float,
  Uint32,
  Uint32,
  Pointer<_FlowNativeBuffer>,
);
typedef _ReleaseNative = Void Function(Pointer<_FlowNativeBuffer>);

@Native<_CreateNative>(symbol: 'fwui_context_create', assetId: _nativeAssetId)
external int _create(Pointer<Pointer<Void>> outContext);

@Native<_DestroyNative>(symbol: 'fwui_context_destroy', assetId: _nativeAssetId)
external void _destroy(Pointer<Void> context);

@Native<_ComposeNative>(
  symbol: 'fwui_compose_flow_demo_v2',
  assetId: _nativeAssetId,
)
external int _compose(
  Pointer<Void> context,
  double width,
  double height,
  int contentCase,
  int pageMode,
  Pointer<_FlowNativeBuffer> output,
);

@Native<_ReleaseNative>(symbol: 'fwui_buffer_release', assetId: _nativeAssetId)
external void _release(Pointer<_FlowNativeBuffer> buffer);

abstract interface class NativeRuntimeClient {
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required int pageMode,
  });

  Future<void> close();
}

final class NativeAssetRuntimeClient implements NativeRuntimeClient {
  NativeAssetRuntimeClient._(this._context);

  static NativeAssetRuntimeClient open() {
    final output = calloc<Pointer<Void>>();
    try {
      _check(_create(output), 'fwui_context_create');
      if (output.value.address == 0) {
        throw StateError('fwui_context_create returned a null context.');
      }
      return NativeAssetRuntimeClient._(output.value);
    } finally {
      calloc.free(output);
    }
  }

  final Pointer<Void> _context;
  bool _closed = false;

  @override
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required int pageMode,
  }) async {
    _ensureOpen();
    final output = calloc<_FlowNativeBuffer>();
    try {
      _check(
        _compose(_context, width, height, contentCase, pageMode, output),
        'fwui_compose_flow_demo_v2',
      );
      if (output.ref.length == 0) return '';
      if (output.ref.data.address == 0) {
        throw const FormatException('Flow result has length but no data.');
      }
      return utf8.decode(output.ref.data.asTypedList(output.ref.length));
    } finally {
      _release(output);
      calloc.free(output);
    }
  }

  @override
  Future<void> close() async {
    if (_closed) return;
    _destroy(_context);
    _closed = true;
  }

  void _ensureOpen() {
    if (_closed) throw StateError('Flow native client is closed.');
  }
}

final class DemoRuntimeClient implements NativeRuntimeClient {
  const DemoRuntimeClient();

  @override
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required int pageMode,
  }) async {
    final placementGroup = contentCase ~/ 3;
    final inlineObjects = placementGroup == 1;
    final floatObjects = placementGroup == 2 || placementGroup == 3;
    final placementMode = switch (placementGroup) {
      1 => 'inline',
      2 => 'float-start',
      3 => 'float-end',
      _ => 'block',
    };
    final baseCase = contentCase % 3;
    final level = baseCase + 1;
    final virtualPages = pageMode == 1;
    final columns = pageMode == 2;
    final paginated = virtualPages || columns;
    final pageHeight = virtualPages ? 240.0 : (columns ? 300.0 : height);
    final fallback = level == 3;
    final pageCount = inlineObjects
        ? 1
        : (virtualPages ? (floatObjects ? 2 : (fallback ? 2 : 3)) : 1);
    final columnCount = columns ? 2 : 1;
    final columnGap = columns ? 24.0 : 0.0;
    final contentWidth = width - 48.0;
    final columnWidth =
        (contentWidth - (columnCount - 1) * columnGap) / columnCount;
    final objectId = level == 3
        ? 'object.missing.level-3'
        : 'image.hero.level-$level';
    final objectWidth = fallback ? 180.0 : 240.0 - (20.0 * baseCase);
    final objectHeight = fallback ? 112.0 : 150.0 - (15.0 * baseCase);
    final floatOnEnd = placementGroup == 3;
    final floatObjectX = floatOnEnd ? width - 32.0 - objectWidth : 32.0;
    final floatTextX = floatOnEnd ? 24.0 : 40.0 + objectWidth;
    final floatTextWidth = contentWidth - objectWidth - 16.0;
    final floatPage = virtualPages ? 1 : 0;
    final floatTextColumn = columns ? 1 : 0;
    final fragments = inlineObjects
        ? <Object?>[
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.inline.level-$level',
              'contentKind': '',
              'pageIndex': 0,
              'columnIndex': 0,
              'bounds': {'x': 24.0, 'y': 32.0, 'width': 42.0, 'height': 56.0},
              'textStart': 0,
              'textEnd': 7,
            },
            {
              'kind': fallback ? 'placeholder' : 'object',
              'sourceItemId': fallback
                  ? 'object.inline-missing.level-3'
                  : 'image.inline.level-$level',
              'contentKind': fallback ? 'unknown' : 'image',
              'pageIndex': 0,
              'columnIndex': 0,
              'bounds': {'x': 69.0, 'y': 42.0, 'width': 72.0, 'height': 36.0},
              'textStart': 0,
              'textEnd': 0,
            },
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.inline.level-$level',
              'contentKind': '',
              'pageIndex': 0,
              'columnIndex': 0,
              'bounds': {'x': 144.0, 'y': 32.0, 'width': 84.0, 'height': 56.0},
              'textStart': 7,
              'textEnd': 21,
            },
          ]
        : floatObjects
        ? <Object?>[
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.intro.level-$level',
              'contentKind': '',
              'pageIndex': 0,
              'columnIndex': 0,
              'bounds': {
                'x': 24.0,
                'y': 32.0,
                'width': columns ? columnWidth : contentWidth,
                'height': 56.0,
              },
              'textStart': 0,
              'textEnd': 52,
            },
            {
              'kind': fallback ? 'placeholder' : 'object',
              'sourceItemId': objectId,
              'contentKind': fallback ? 'unknown' : 'image',
              'pageIndex': floatPage,
              'columnIndex': 0,
              'bounds': {
                'x': columns ? 32.0 : floatObjectX,
                'y': virtualPages ? 40.0 : 104.0,
                'width': objectWidth,
                'height': objectHeight,
              },
              'textStart': 0,
              'textEnd': 0,
            },
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.closing.level-$level',
              'contentKind': '',
              'pageIndex': floatPage,
              'columnIndex': floatTextColumn,
              'bounds': {
                'x': columns ? 24.0 + columnWidth + columnGap : floatTextX,
                'y': virtualPages ? 50.0 : (columns ? 34.0 : 114.0),
                'width': columns ? columnWidth : floatTextWidth,
                'height': 56.0,
              },
              'textStart': 0,
              'textEnd': 58,
            },
          ]
        : <Object?>[
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.intro.level-$level',
              'contentKind': '',
              'pageIndex': 0,
              'columnIndex': 0,
              'bounds': {
                'x': 24.0,
                'y': 32.0,
                'width': columns ? columnWidth : contentWidth,
                'height': 56.0,
              },
              'textStart': 0,
              'textEnd': 52,
            },
            {
              'kind': fallback ? 'placeholder' : 'object',
              'sourceItemId': objectId,
              'contentKind': fallback ? 'unknown' : 'image',
              'pageIndex': virtualPages && !fallback ? 1 : 0,
              'columnIndex': 0,
              'bounds': {
                'x': 24.0,
                'y': virtualPages && !fallback ? 40.0 : 104.0,
                'width': objectWidth,
                'height': objectHeight,
              },
              'textStart': 0,
              'textEnd': 0,
            },
            {
              'kind': 'text',
              'sourceItemId': 'paragraph.closing.level-$level',
              'contentKind': '',
              'pageIndex': virtualPages ? (fallback ? 1 : 2) : 0,
              'columnIndex': columns ? 1 : 0,
              'bounds': {
                'x': columns ? 24.0 + columnWidth + columnGap : 24.0,
                'y': paginated ? 34.0 : 258.0,
                'width': columns ? columnWidth : contentWidth,
                'height': 56.0,
              },
              'textStart': 0,
              'textEnd': 58,
            },
          ];
    return jsonEncode({
      'pluginId': 'org.facetwire.reference.flow-layout',
      'capability': 'facetwire.layout.flow',
      'interfaceVersion': 1,
      'demoCase': contentCase + (pageMode * 12),
      'contentCase': contentCase,
      'pageMode': pageMode,
      'inlineObjects': inlineObjects,
      'placementMode': placementMode,
      'composeStatus': 0,
      'complete': true,
      'pageCount': pageCount,
      'fragmentCount': 3,
      'textFragmentCount': 2,
      'objectFragmentCount': 1,
      'continuousExtent': {
        'width': width,
        'height': paginated ? pageHeight * pageCount : height,
      },
      'pageSize': {'width': width, 'height': pageHeight},
      'pageGap': 0.0,
      'columnCount': columnCount,
      'columnGap': columnGap,
      'contentBounds': {
        'x': 24.0,
        'y': 24.0,
        'width': contentWidth,
        'height': pageHeight - 48.0,
      },
      'planKey': '00000000000000000000000000000000',
      'pagesBalanced': true,
      'supportedSlice':
          'continuous+virtual-pages+columns+block+inline+float-start+float-end',
      'nativeRuntime': false,
      'fragments': fragments,
    });
  }

  @override
  Future<void> close() async {}
}

void _check(int status, String operation) {
  if (status != 0) {
    throw StateError('$operation failed with native status $status.');
  }
}
