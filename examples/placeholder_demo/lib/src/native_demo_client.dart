// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'demo_models.dart';

final class _NativeBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

final class _NativeRequest extends Struct {
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

typedef _CreateNative = Int32 Function(Pointer<Pointer<Void>>);
typedef _CreateDart = int Function(Pointer<Pointer<Void>>);
typedef _DestroyNative = Void Function(Pointer<Void>);
typedef _DestroyDart = void Function(Pointer<Void>);
typedef _BufferOperationNative = Int32 Function(
  Pointer<Void>,
  Pointer<_NativeBuffer>,
);
typedef _BufferOperationDart = int Function(
  Pointer<Void>,
  Pointer<_NativeBuffer>,
);
typedef _RenderNative = Int32 Function(
  Pointer<Void>,
  Pointer<_NativeRequest>,
  Pointer<_NativeBuffer>,
);
typedef _RenderDart = int Function(
  Pointer<Void>,
  Pointer<_NativeRequest>,
  Pointer<_NativeBuffer>,
);
typedef _HitTestNative = Int32 Function(
  Pointer<Void>,
  Pointer<_NativeRequest>,
  Float,
  Float,
  Pointer<Uint32>,
  Pointer<Uint32>,
);
typedef _HitTestDart = int Function(
  Pointer<Void>,
  Pointer<_NativeRequest>,
  double,
  double,
  Pointer<Uint32>,
  Pointer<Uint32>,
);
typedef _ReleaseNative = Void Function(Pointer<_NativeBuffer>);
typedef _ReleaseDart = void Function(Pointer<_NativeBuffer>);

final class FfiNativeDemoClient implements NativeDemoClient {
  FfiNativeDemoClient._(DynamicLibrary library, this._context)
    : _destroy = library.lookupFunction<_DestroyNative, _DestroyDart>(
        'fwdemo_context_destroy',
      ),
      _snapshot = library
          .lookupFunction<_BufferOperationNative, _BufferOperationDart>(
            'fwdemo_runtime_snapshot',
          ),
      _schema = library
          .lookupFunction<_BufferOperationNative, _BufferOperationDart>(
            'fwdemo_parameter_schema',
          ),
      _render = library.lookupFunction<_RenderNative, _RenderDart>(
        'fwdemo_render',
      ),
      _hitTest = library.lookupFunction<_HitTestNative, _HitTestDart>(
        'fwdemo_hit_test',
      ),
      _release = library.lookupFunction<_ReleaseNative, _ReleaseDart>(
        'fwdemo_buffer_release',
      );

  static FfiNativeDemoClient open({String? libraryPath}) {
    final library = DynamicLibrary.open(libraryPath ?? _defaultLibraryPath());
    final create = library.lookupFunction<_CreateNative, _CreateDart>(
      'fwdemo_context_create',
    );
    final output = calloc<Pointer<Void>>();
    try {
      _check(create(output), 'fwdemo_context_create');
      return FfiNativeDemoClient._(library, output.value);
    } finally {
      calloc.free(output);
    }
  }

  final Pointer<Void> _context;
  final _DestroyDart _destroy;
  final _BufferOperationDart _snapshot;
  final _BufferOperationDart _schema;
  final _RenderDart _render;
  final _HitTestDart _hitTest;
  final _ReleaseDart _release;
  bool _closed = false;

  @override
  Future<Map<String, Object?>> snapshot() async =>
      jsonDecode(_readBuffer(_snapshot, 'fwdemo_runtime_snapshot'))
          as Map<String, Object?>;

  @override
  Future<Map<String, Object?>> parameterSchema() async =>
      jsonDecode(_readBuffer(_schema, 'fwdemo_parameter_schema'))
          as Map<String, Object?>;

  @override
  Future<DemoRenderReport> render(DemoNativeRequest request) async {
    _ensureOpen();
    return _withRequest(request, (nativeRequest) {
      final output = calloc<_NativeBuffer>();
      try {
        _check(_render(_context, nativeRequest, output), 'fwdemo_render');
        return DemoRenderReport.fromJsonString(_copyUtf8(output.ref));
      } finally {
        _release(output);
        calloc.free(output);
      }
    });
  }

  @override
  Future<int> hitTest(DemoNativeRequest request, double x, double y) async {
    _ensureOpen();
    return _withRequest(request, (nativeRequest) {
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
  }

  @override
  Future<void> close() async {
    if (!_closed) {
      _destroy(_context);
      _closed = true;
    }
  }

  String _readBuffer(_BufferOperationDart operation, String name) {
    _ensureOpen();
    final output = calloc<_NativeBuffer>();
    try {
      _check(operation(_context, output), name);
      return _copyUtf8(output.ref);
    } finally {
      _release(output);
      calloc.free(output);
    }
  }

  T _withRequest<T>(
    DemoNativeRequest request,
    T Function(Pointer<_NativeRequest>) operation,
  ) {
    final nativeRequest = calloc<_NativeRequest>();
    final kindBytes = utf8.encode(request.contentKind);
    final labelBytes = utf8.encode(request.label);
    final kind = request.contentKind.toNativeUtf8();
    final label = request.label.toNativeUtf8();
    try {
      nativeRequest.ref
        ..structSize = sizeOf<_NativeRequest>()
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
      return operation(nativeRequest);
    } finally {
      calloc.free(kind);
      calloc.free(label);
      calloc.free(nativeRequest);
    }
  }

  void _ensureOpen() {
    if (_closed) {
      throw StateError('Native demo client is closed.');
    }
  }

  static String _copyUtf8(_NativeBuffer buffer) {
    if (buffer.length == 0) {
      return '';
    }
    if (buffer.data.address == 0) {
      throw const FormatException('Native buffer has length but no data.');
    }
    return utf8.decode(buffer.data.asTypedList(buffer.length));
  }

  static void _check(int status, String operation) {
    if (status != 0) {
      throw StateError('$operation failed with FacetWire status $status.');
    }
  }

  static String _defaultLibraryPath() {
    final override = Platform.environment['FACETWIRE_PLACEHOLDER_DEMO_BRIDGE'];
    if (override != null && override.isNotEmpty) {
      return override;
    }
    final executable = File(Platform.resolvedExecutable);
    if (Platform.isWindows) {
      return '${executable.parent.path}\\facetwire_placeholder_demo_bridge.dll';
    }
    if (Platform.isMacOS) {
      return '${executable.parent.parent.path}/Frameworks/'
          'libfacetwire_placeholder_demo_bridge.dylib';
    }
    throw UnsupportedError('This demo currently targets Windows and macOS.');
  }
}

final class UnavailableNativeDemoClient implements NativeDemoClient {
  const UnavailableNativeDemoClient(this.message);
  final String message;

  Never _fail() => throw StateError(message);

  @override
  Future<void> close() async {}

  @override
  Future<int> hitTest(DemoNativeRequest request, double x, double y) async =>
      _fail();

  @override
  Future<Map<String, Object?>> parameterSchema() async => _fail();

  @override
  Future<DemoRenderReport> render(DemoNativeRequest request) async => _fail();

  @override
  Future<Map<String, Object?>> snapshot() async => _fail();
}
