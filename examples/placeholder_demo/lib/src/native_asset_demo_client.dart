// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'demo_models.dart';

const _assetId =
    'package:facetwire_placeholder_demo/facetwire_placeholder_demo_bridge.dart';

final class _AssetBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

final class _AssetRequest extends Struct {
  @Uint32()
  external int structSize;
  @Float()
  external double width;
  @Float()
  external double height;
  @Float()
  external double opacity;
  @Float()
  external double backgroundAlpha;
  @Float()
  external double fontScale;
  @Float()
  external double devicePixelRatio;
  @Uint32()
  external int reason;
  @Uint32()
  external int mode;
  @Uint32()
  external int permittedActions;
  @Uint32()
  external int phase;
  @Uint32()
  external int progressKind;
  @Uint32()
  external int stale;
  @Uint32()
  external int prefersDark;
  @Uint32()
  external int highContrast;
  @Uint32()
  external int reduceMotion;
  @Uint32()
  external int measureCase;
  @Uint64()
  external int completed;
  @Uint64()
  external int total;
  @Uint64()
  external int presentationRevision;
  external Pointer<Utf8> contentKindUtf8;
  @Uint64()
  external int contentKindLength;
  external Pointer<Utf8> labelUtf8;
  @Uint64()
  external int labelLength;
}

typedef _Create = Int32 Function(Pointer<Pointer<Void>>);
typedef _Destroy = Void Function(Pointer<Void>);
typedef _BufferOperation = Int32 Function(Pointer<Void>, Pointer<_AssetBuffer>);
typedef _Render = Int32 Function(
  Pointer<Void>,
  Pointer<_AssetRequest>,
  Pointer<_AssetBuffer>,
);
typedef _HitTest = Int32 Function(
  Pointer<Void>,
  Pointer<_AssetRequest>,
  Float,
  Float,
  Pointer<Uint32>,
  Pointer<Uint32>,
);
typedef _Release = Void Function(Pointer<_AssetBuffer>);

@Native<_Create>(symbol: 'fwdemo_context_create', assetId: _assetId)
external int _create(Pointer<Pointer<Void>> output);
@Native<_Destroy>(symbol: 'fwdemo_context_destroy', assetId: _assetId)
external void _destroy(Pointer<Void> context);
@Native<_BufferOperation>(symbol: 'fwdemo_runtime_snapshot', assetId: _assetId)
external int _snapshot(Pointer<Void> context, Pointer<_AssetBuffer> output);
@Native<_BufferOperation>(symbol: 'fwdemo_parameter_schema', assetId: _assetId)
external int _schema(Pointer<Void> context, Pointer<_AssetBuffer> output);
@Native<_Render>(symbol: 'fwdemo_render', assetId: _assetId)
external int _render(
  Pointer<Void> context,
  Pointer<_AssetRequest> request,
  Pointer<_AssetBuffer> output,
);
@Native<_HitTest>(symbol: 'fwdemo_hit_test', assetId: _assetId)
external int _hitTest(
  Pointer<Void> context,
  Pointer<_AssetRequest> request,
  double x,
  double y,
  Pointer<Uint32> hit,
  Pointer<Uint32> action,
);
@Native<_Release>(symbol: 'fwdemo_buffer_release', assetId: _assetId)
external void _release(Pointer<_AssetBuffer> buffer);

final class NativeAssetDemoClient implements NativeDemoClient {
  NativeAssetDemoClient._(this._context);

  static NativeAssetDemoClient open() {
    final output = calloc<Pointer<Void>>();
    try {
      _check(_create(output), 'fwdemo_context_create');
      if (output.value.address == 0) {
        throw StateError('fwdemo_context_create returned a null context.');
      }
      return NativeAssetDemoClient._(output.value);
    } finally {
      calloc.free(output);
    }
  }

  final Pointer<Void> _context;
  bool _closed = false;

  @override
  Future<Map<String, Object?>> snapshot() async =>
      jsonDecode(_read(_snapshot, 'fwdemo_runtime_snapshot'))
          as Map<String, Object?>;

  @override
  Future<Map<String, Object?>> parameterSchema() async =>
      jsonDecode(_read(_schema, 'fwdemo_parameter_schema'))
          as Map<String, Object?>;

  @override
  Future<DemoRenderReport> render(DemoNativeRequest request) async =>
      _withRequest(request, (nativeRequest) {
        final output = calloc<_AssetBuffer>();
        try {
          _check(_render(_context, nativeRequest, output), 'fwdemo_render');
          return DemoRenderReport.fromJsonString(_copy(output.ref));
        } finally {
          _release(output);
          calloc.free(output);
        }
      });

  @override
  Future<int> hitTest(DemoNativeRequest request, double x, double y) async =>
      _withRequest(request, (nativeRequest) {
        final hit = calloc<Uint32>();
        final action = calloc<Uint32>();
        try {
          _check(
            _hitTest(_context, nativeRequest, x, y, hit, action),
            'fwdemo_hit_test',
          );
          return hit.value == 0 ? 0 : action.value;
        } finally {
          calloc.free(hit);
          calloc.free(action);
        }
      });

  String _read(
    int Function(Pointer<Void>, Pointer<_AssetBuffer>) operation,
    String name,
  ) {
    _ensureOpen();
    final output = calloc<_AssetBuffer>();
    try {
      _check(operation(_context, output), name);
      return _copy(output.ref);
    } finally {
      _release(output);
      calloc.free(output);
    }
  }

  T _withRequest<T>(
    DemoNativeRequest request,
    T Function(Pointer<_AssetRequest>) operation,
  ) {
    _ensureOpen();
    final native = calloc<_AssetRequest>();
    final kindBytes = utf8.encode(request.contentKind);
    final labelBytes = utf8.encode(request.label);
    final kind = request.contentKind.toNativeUtf8();
    final label = request.label.toNativeUtf8();
    try {
      native.ref
        ..structSize = sizeOf<_AssetRequest>()
        ..width = request.width
        ..height = request.height
        ..opacity = request.opacity
        ..backgroundAlpha = request.backgroundAlpha
        ..fontScale = request.fontScale
        ..devicePixelRatio = request.devicePixelRatio
        ..reason = request.reason.wireValue
        ..mode = request.mode.wireValue
        ..permittedActions = request.permittedActions
        ..phase = request.phase.wireValue
        ..progressKind = request.progressKind
        ..stale = request.stale ? 1 : 0
        ..prefersDark = request.prefersDark ? 1 : 0
        ..highContrast = request.highContrast ? 1 : 0
        ..reduceMotion = request.reduceMotion ? 1 : 0
        ..measureCase = request.measureCase.wireValue
        ..completed = request.completed
        ..total = request.total
        ..presentationRevision = request.presentationRevision
        ..contentKindUtf8 = kind
        ..contentKindLength = kindBytes.length
        ..labelUtf8 = label
        ..labelLength = labelBytes.length;
      return operation(native);
    } finally {
      calloc.free(kind);
      calloc.free(label);
      calloc.free(native);
    }
  }

  @override
  Future<void> close() async {
    if (_closed) return;
    _destroy(_context);
    _closed = true;
  }

  void _ensureOpen() {
    if (_closed) throw StateError('Native asset demo client is closed.');
  }
}

String _copy(_AssetBuffer buffer) {
  if (buffer.length == 0) return '';
  if (buffer.data.address == 0) {
    throw const FormatException('Native buffer has length but no data.');
  }
  return utf8.decode(buffer.data.asTypedList(buffer.length));
}

void _check(int status, String operation) {
  if (status != 0) {
    throw StateError('$operation failed with FacetWire status $status.');
  }
}
