// SPDX-License-Identifier: MPL-2.0
// Desktop-only, explicitly pinned native plugin -> bounded declarative view.
// Loading a native library executes in-process code; this is not a sandbox.
import 'dart:ffi';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:ffi/ffi.dart';

import 'declarative_renderer_pack.dart';

final class _StringView extends Struct {
  external Pointer<Char> data;

  @UintPtr()
  external int length;
}

typedef _NativeLoad = Int32 Function(
  _StringView,
  _StringView,
  Pointer<Uint8>,
  UintPtr,
  Pointer<UintPtr>,
);
typedef _DartLoad = int Function(
  _StringView,
  _StringView,
  Pointer<Uint8>,
  int,
  Pointer<UintPtr>,
);

final class NativeRendererPack {
  NativeRendererPack._();

  static Future<DeclarativeRendererPack> load({
    required String bridgeLibraryPath,
    required String bridgeSha256,
    required String pluginLibraryPath,
    required String pluginSha256,
    required String expectedPluginId,
  }) async {
    if (!(Platform.isWindows || Platform.isLinux || Platform.isMacOS)) {
      throw UnsupportedError('Runtime native plugins are desktop-only');
    }
    if (!RegExp(r'^[a-zA-Z0-9][a-zA-Z0-9.-]{0,127}$')
        .hasMatch(expectedPluginId)) {
      throw const FormatException('Exact plugin identity required');
    }
    await _pin(bridgeLibraryPath, bridgeSha256);
    await _pin(pluginLibraryPath, pluginSha256);
    final bridge = DynamicLibrary.open(bridgeLibraryPath);
    final load = bridge.lookupFunction<_NativeLoad, _DartLoad>(
      'fw_flutter_zone_profile_load',
    );
    final path = pluginLibraryPath.toNativeUtf8();
    final identity = expectedPluginId.toNativeUtf8();
    final pathView = calloc<_StringView>();
    final identityView = calloc<_StringView>();
    final output = calloc<Uint8>(65536);
    final length = calloc<UintPtr>();
    try {
      pathView.ref
        ..data = path.cast<Char>()
        ..length = path.length;
      identityView.ref
        ..data = identity.cast<Char>()
        ..length = identity.length;
      final status = load(
        pathView.ref,
        identityView.ref,
        output,
        65536,
        length,
      );
      if (status != 0) {
        throw FormatException(
          'Native renderer bridge rejected plugin: $status',
        );
      }
      if (length.value < 1 || length.value > 65536) {
        throw const FormatException(
          'Native renderer returned invalid profile length',
        );
      }
      // The native bridge copies the bytes before unloading the plugin; the
      // Dart parser still rejects malformed grammar and built-in overrides.
      return DeclarativeRendererPack.parse(output.asTypedList(length.value));
    } finally {
      calloc.free(length);
      calloc.free(output);
      calloc.free(identityView);
      calloc.free(pathView);
      calloc.free(identity);
      calloc.free(path);
    }
  }

  static Future<void> _pin(String path, String expectedSha256) async {
    if (!RegExp(r'^[0-9a-f]{64}$').hasMatch(expectedSha256)) {
      throw const FormatException('Pinned renderer SHA-256 required');
    }
    final absolute = Platform.isWindows
        ? RegExp(r'^[A-Za-z]:[\\/]').hasMatch(path) || path.startsWith(r'\\')
        : path.startsWith('/');
    if (!absolute ||
        await FileSystemEntity.type(path, followLinks: false) !=
            FileSystemEntityType.file) {
      throw const FormatException('Regular absolute native library required');
    }
    final file = File(path);
    final before = await file.length();
    if (before < 1 || before > 134217728) {
      throw const FormatException('Native library size exceeds policy');
    }
    final observed = await sha256.bind(file.openRead()).first;
    if (await file.length() != before ||
        observed.toString() != expectedSha256) {
      throw const FormatException('Native library digest changed');
    }
  }
}
