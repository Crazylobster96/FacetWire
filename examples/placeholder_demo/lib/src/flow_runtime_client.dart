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
  Pointer<_FlowNativeBuffer>,
);
typedef _ReleaseNative = Void Function(Pointer<_FlowNativeBuffer>);

@Native<_CreateNative>(symbol: 'fwui_context_create', assetId: _nativeAssetId)
external int _create(Pointer<Pointer<Void>> outContext);

@Native<_DestroyNative>(symbol: 'fwui_context_destroy', assetId: _nativeAssetId)
external void _destroy(Pointer<Void> context);

@Native<_ComposeNative>(
  symbol: 'fwui_compose_flow_demo',
  assetId: _nativeAssetId,
)
external int _compose(
  Pointer<Void> context,
  double width,
  double height,
  int demoCase,
  Pointer<_FlowNativeBuffer> output,
);

@Native<_ReleaseNative>(symbol: 'fwui_buffer_release', assetId: _nativeAssetId)
external void _release(Pointer<_FlowNativeBuffer> buffer);

abstract interface class NativeRuntimeClient {
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int demoCase,
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
    required int demoCase,
  }) async {
    _ensureOpen();
    final output = calloc<_FlowNativeBuffer>();
    try {
      _check(
        _compose(_context, width, height, demoCase, output),
        'fwui_compose_flow_demo',
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
    required int demoCase,
  }) async {
    final contentCase = demoCase == 3 ? 0 : demoCase.clamp(0, 2);
    final level = contentCase + 1;
    final unsupported = demoCase == 3;
    final objectId = level == 3
        ? 'object.missing.level-3'
        : 'image.hero.level-$level';
    return jsonEncode({
      'pluginId': 'org.facetwire.reference.flow-layout',
      'capability': 'facetwire.layout.flow',
      'interfaceVersion': 1,
      'demoCase': demoCase,
      'composeStatus': unsupported ? 11 : 0,
      'complete': !unsupported,
      'pageCount': unsupported ? 0 : 1,
      'fragmentCount': unsupported ? 0 : 3,
      'textFragmentCount': unsupported ? 0 : 2,
      'objectFragmentCount': unsupported ? 0 : 1,
      'continuousExtent': {'width': width, 'height': height},
      'planKey': '00000000000000000000000000000000',
      'pagesBalanced': true,
      'supportedSlice': 'continuous+block',
      'nativeRuntime': false,
      'fragments': unsupported
          ? const <Object>[]
          : [
              {
                'kind': 'text',
                'sourceItemId': 'paragraph.intro.level-$level',
                'contentKind': '',
                'bounds': {
                  'x': 24.0,
                  'y': 32.0,
                  'width': width - 48,
                  'height': 56.0,
                },
                'textStart': 0,
                'textEnd': 52,
              },
              {
                'kind': level == 3 ? 'placeholder' : 'object',
                'sourceItemId': objectId,
                'contentKind': level == 3 ? 'unknown' : 'image',
                'bounds': {
                  'x': 24.0,
                  'y': 104.0,
                  'width': 220.0,
                  'height': 138.0,
                },
                'textStart': 0,
                'textEnd': 0,
              },
              {
                'kind': 'text',
                'sourceItemId': 'paragraph.closing.level-$level',
                'contentKind': '',
                'bounds': {
                  'x': 24.0,
                  'y': 258.0,
                  'width': width - 48,
                  'height': 56.0,
                },
                'textStart': 0,
                'textEnd': 58,
              },
            ],
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
