// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'models.dart';

final class _NativeBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

typedef _CreateNative = Int32 Function(Pointer<Pointer<Void>>);
typedef _CreateDart = int Function(Pointer<Pointer<Void>>);
typedef _DestroyNative = Void Function(Pointer<Void>);
typedef _DestroyDart = void Function(Pointer<Void>);
typedef _SnapshotNative = Int32 Function(
  Pointer<Void>,
  Pointer<_NativeBuffer>,
);
typedef _SnapshotDart = int Function(
  Pointer<Void>,
  Pointer<_NativeBuffer>,
);
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
typedef _ReleaseNative = Void Function(Pointer<_NativeBuffer>);
typedef _ReleaseDart = void Function(Pointer<_NativeBuffer>);

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
        _release = library.lookupFunction<_ReleaseNative, _ReleaseDart>(
          'fwui_buffer_release',
        );

  static FfiNativeRuntimeClient open({String? libraryPath}) {
    final library = libraryPath == null
        ? _openDefaultLibrary()
        : DynamicLibrary.open(libraryPath);
    final create =
        library.lookupFunction<_CreateNative, _CreateDart>('fwui_context_create');
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
  final _ReleaseDart _release;
  bool _closed = false;

  @override
  Future<String> snapshot() async {
    _ensureOpen();
    final output = calloc<_NativeBuffer>();
    try {
      _checkStatus(_snapshot(_context, output), 'fwui_runtime_snapshot');
      return utf8.decode(_copyBytes(output.ref));
    } finally {
      _release(output);
      calloc.free(output);
    }
  }

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
  Future<void> close() async {
    if (!_closed) {
      _destroy(_context);
      _closed = true;
    }
  }

  void _ensureOpen() {
    if (_closed) {
      throw StateError('Native runtime client is closed.');
    }
  }

  static Uint8List _copyBytes(_NativeBuffer buffer) {
    if (buffer.length == 0) {
      return Uint8List(0);
    }
    if (buffer.data.address == 0) {
      throw const FormatException('Native buffer has length but no data.');
    }
    return Uint8List.fromList(buffer.data.asTypedList(buffer.length));
  }

  static void _checkStatus(int status, String operation) {
    if (status != 0) {
      throw StateError('$operation failed with native status $status.');
    }
  }

  static DynamicLibrary _openDefaultLibrary() {
    if (Platform.isIOS) {
      return DynamicLibrary.process();
    }
    if (Platform.isWindows) {
      return DynamicLibrary.open('facetwire_ui_spike.dll');
    }
    if (Platform.isMacOS) {
      return DynamicLibrary.open('libfacetwire_ui_spike.dylib');
    }
    return DynamicLibrary.open('libfacetwire_ui_spike.so');
  }
}
