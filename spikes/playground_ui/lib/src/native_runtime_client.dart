// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'models.dart';

const _nativeAssetId =
    'package:facetwire_playground_ui_spike/facetwire_ui_spike.dart';

final class _NativeBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

typedef _CreateNative = Int32 Function(Pointer<Pointer<Void>>);
typedef _CreateDart = int Function(Pointer<Pointer<Void>>);
typedef _DestroyNative = Void Function(Pointer<Void>);
typedef _DestroyDart = void Function(Pointer<Void>);
typedef _SnapshotNative = Int32 Function(Pointer<Void>, Pointer<_NativeBuffer>);
typedef _SnapshotDart = int Function(Pointer<Void>, Pointer<_NativeBuffer>);
typedef _RenderNative = Int32 Function(
  Pointer<Void>,
  Float,
  Float,
  Float,
  Pointer<_NativeBuffer>,
  Pointer<_NativeBuffer>,
);
typedef _RenderDart = int Function(
  Pointer<Void>,
  double,
  double,
  double,
  Pointer<_NativeBuffer>,
  Pointer<_NativeBuffer>,
);
typedef _FlowNative = Int32 Function(
  Pointer<Void>,
  Float,
  Float,
  Uint32,
  Uint32,
  Pointer<_NativeBuffer>,
);
typedef _FlowDart = int Function(
  Pointer<Void>,
  double,
  double,
  int,
  int,
  Pointer<_NativeBuffer>,
);
typedef _ReleaseNative = Void Function(Pointer<_NativeBuffer>);
typedef _ReleaseDart = void Function(Pointer<_NativeBuffer>);

@Native<_CreateNative>(symbol: 'fwui_context_create', assetId: _nativeAssetId)
external int _assetCreate(Pointer<Pointer<Void>> outContext);

@Native<_DestroyNative>(symbol: 'fwui_context_destroy', assetId: _nativeAssetId)
external void _assetDestroy(Pointer<Void> context);

@Native<_SnapshotNative>(
  symbol: 'fwui_runtime_snapshot',
  assetId: _nativeAssetId,
)
external int _assetSnapshot(
  Pointer<Void> context,
  Pointer<_NativeBuffer> output,
);

@Native<_RenderNative>(
  symbol: 'fwui_render_placeholder',
  assetId: _nativeAssetId,
)
external int _assetRender(
  Pointer<Void> context,
  double width,
  double height,
  double opacity,
  Pointer<_NativeBuffer> display,
  Pointer<_NativeBuffer> semantics,
);

@Native<_FlowNative>(
  symbol: 'fwui_compose_flow_demo_v2',
  assetId: _nativeAssetId,
)
external int _assetComposeFlow(
  Pointer<Void> context,
  double width,
  double height,
  int contentCase,
  int pageMode,
  Pointer<_NativeBuffer> output,
);

@Native<_ReleaseNative>(symbol: 'fwui_buffer_release', assetId: _nativeAssetId)
external void _assetRelease(Pointer<_NativeBuffer> buffer);

final class NativeAssetRuntimeClient implements NativeRuntimeClient {
  NativeAssetRuntimeClient._(this._context);

  static NativeAssetRuntimeClient open() {
    final outContext = calloc<Pointer<Void>>();
    try {
      _checkStatus(_assetCreate(outContext), 'fwui_context_create');
      if (outContext.value.address == 0) {
        throw StateError('fwui_context_create returned a null context.');
      }
      return NativeAssetRuntimeClient._(outContext.value);
    } finally {
      calloc.free(outContext);
    }
  }

  final Pointer<Void> _context;
  bool _closed = false;

  @override
  Future<String> snapshot() async => _singleBuffer(
    'fwui_runtime_snapshot',
    (output) => _assetSnapshot(_context, output),
  );

  @override
  Future<RenderFrame> renderPlaceholder({
    required double width,
    required double height,
    required double opacity,
  }) async {
    _ensureOpen();
    final display = calloc<_NativeBuffer>();
    final semantics = calloc<_NativeBuffer>();
    try {
      _checkStatus(
        _assetRender(_context, width, height, opacity, display, semantics),
        'fwui_render_placeholder',
      );
      return RenderFrame(
        displayList: _copyBytes(display.ref),
        semanticsJson: utf8.decode(_copyBytes(semantics.ref)),
      );
    } finally {
      _assetRelease(display);
      _assetRelease(semantics);
      calloc.free(display);
      calloc.free(semantics);
    }
  }

  @override
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required bool virtualPages,
  }) => _singleBuffer(
    'fwui_compose_flow_demo_v2',
    (output) => _assetComposeFlow(
      _context,
      width,
      height,
      contentCase,
      virtualPages ? 1 : 0,
      output,
    ),
  );

  Future<String> _singleBuffer(
    String operation,
    int Function(Pointer<_NativeBuffer>) invoke,
  ) async {
    _ensureOpen();
    final output = calloc<_NativeBuffer>();
    try {
      _checkStatus(invoke(output), operation);
      return utf8.decode(_copyBytes(output.ref));
    } finally {
      _assetRelease(output);
      calloc.free(output);
    }
  }

  @override
  Future<void> close() async {
    if (!_closed) {
      _assetDestroy(_context);
      _closed = true;
    }
  }

  void _ensureOpen() {
    if (_closed) throw StateError('Native asset runtime client is closed.');
  }
}

final class FfiNativeRuntimeClient implements NativeRuntimeClient {
  FfiNativeRuntimeClient._(DynamicLibrary library, this._context)
    : _destroy = library.lookupFunction<_DestroyNative, _DestroyDart>(
        'fwui_context_destroy',
      ),
      _snapshot = library.lookupFunction<_SnapshotNative, _SnapshotDart>(
        'fwui_runtime_snapshot',
      ),
      _render = library.lookupFunction<_RenderNative, _RenderDart>(
        'fwui_render_placeholder',
      ),
      _flow = library.lookupFunction<_FlowNative, _FlowDart>(
        'fwui_compose_flow_demo_v2',
      ),
      _release = library.lookupFunction<_ReleaseNative, _ReleaseDart>(
        'fwui_buffer_release',
      );

  static FfiNativeRuntimeClient open({String? libraryPath}) {
    final library = libraryPath == null
        ? _openDefaultLibrary()
        : DynamicLibrary.open(libraryPath);
    final create = library.lookupFunction<_CreateNative, _CreateDart>(
      'fwui_context_create',
    );
    final outContext = calloc<Pointer<Void>>();
    try {
      _checkStatus(create(outContext), 'fwui_context_create');
      return FfiNativeRuntimeClient._(library, outContext.value);
    } finally {
      calloc.free(outContext);
    }
  }

  final Pointer<Void> _context;
  final _DestroyDart _destroy;
  final _SnapshotDart _snapshot;
  final _RenderDart _render;
  final _FlowDart _flow;
  final _ReleaseDart _release;
  bool _closed = false;

  @override
  Future<String> snapshot() async => _singleBuffer(
    'fwui_runtime_snapshot',
    (output) => _snapshot(_context, output),
  );

  @override
  Future<RenderFrame> renderPlaceholder({
    required double width,
    required double height,
    required double opacity,
  }) async {
    _ensureOpen();
    final display = calloc<_NativeBuffer>();
    final semantics = calloc<_NativeBuffer>();
    try {
      _checkStatus(
        _render(_context, width, height, opacity, display, semantics),
        'fwui_render_placeholder',
      );
      return RenderFrame(
        displayList: _copyBytes(display.ref),
        semanticsJson: utf8.decode(_copyBytes(semantics.ref)),
      );
    } finally {
      _release(display);
      _release(semantics);
      calloc.free(display);
      calloc.free(semantics);
    }
  }

  @override
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required bool virtualPages,
  }) => _singleBuffer(
    'fwui_compose_flow_demo_v2',
    (output) => _flow(
      _context,
      width,
      height,
      contentCase,
      virtualPages ? 1 : 0,
      output,
    ),
  );

  Future<String> _singleBuffer(
    String operation,
    int Function(Pointer<_NativeBuffer>) invoke,
  ) async {
    _ensureOpen();
    final output = calloc<_NativeBuffer>();
    try {
      _checkStatus(invoke(output), operation);
      return utf8.decode(_copyBytes(output.ref));
    } finally {
      _release(output);
      calloc.free(output);
    }
  }

  @override
  Future<void> close() async {
    if (!_closed) {
      _destroy(_context);
      _closed = true;
    }
  }

  void _ensureOpen() {
    if (_closed) throw StateError('Native demo client is closed.');
  }

  static DynamicLibrary _openDefaultLibrary() {
    if (Platform.isIOS) return DynamicLibrary.process();
    if (Platform.isWindows) {
      return DynamicLibrary.open('facetwire_ui_spike.dll');
    }
    if (Platform.isMacOS) {
      return DynamicLibrary.open('libfacetwire_ui_spike.dylib');
    }
    return DynamicLibrary.open('libfacetwire_ui_spike.so');
  }
}

Uint8List _copyBytes(_NativeBuffer buffer) {
  if (buffer.length == 0) return Uint8List(0);
  if (buffer.data.address == 0) {
    throw const FormatException('Native buffer has length but no data.');
  }
  return Uint8List.fromList(buffer.data.asTypedList(buffer.length));
}

void _checkStatus(int status, String operation) {
  if (status != 0) {
    throw StateError('$operation failed with native status $status.');
  }
}
