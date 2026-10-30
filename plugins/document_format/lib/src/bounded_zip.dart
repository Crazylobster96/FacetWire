// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive.dart' show getCrc32;

import 'model.dart';

/// OPC ZIP reader. No filesystem extraction, no ZIP64, no unbounded inflate.
final class BoundedZip {
  BoundedZip(this.bytes, this.limits) {
    if (bytes.length < 22 || bytes.length > limits.maxInputBytes) _bad();
    final data = ByteData.sublistView(bytes);
    int? end;
    for (var p = bytes.length - 22; p >= 0 && p >= bytes.length - 65557; p--) {
      if (data.getUint32(p, Endian.little) == 0x06054b50 &&
          p + 22 + data.getUint16(p + 20, Endian.little) == bytes.length) {
        end = p;
        break;
      }
    }
    if (end == null) _bad();
    final e = end;
    final count = data.getUint16(e + 10, Endian.little);
    var p = data.getUint32(e + 16, Endian.little);
    final directoryEnd = p + data.getUint32(e + 12, Endian.little);
    if (data.getUint16(e + 4, Endian.little) != 0 ||
        data.getUint16(e + 6, Endian.little) != 0 ||
        data.getUint16(e + 8, Endian.little) != count ||
        count == 65535 ||
        count > limits.maxEntries ||
        directoryEnd > e)
      _bad();
    var total = 0;
    for (var i = 0; i < count; i++) {
      if (p + 46 > directoryEnd ||
          data.getUint32(p, Endian.little) != 0x02014b50)
        _bad();
      final flags = data.getUint16(p + 8, Endian.little);
      final method = data.getUint16(p + 10, Endian.little);
      final crc = data.getUint32(p + 16, Endian.little);
      final compressed = data.getUint32(p + 20, Endian.little);
      final size = data.getUint32(p + 24, Endian.little);
      final nameSize = data.getUint16(p + 28, Endian.little);
      final extra = data.getUint16(p + 30, Endian.little);
      final comment = data.getUint16(p + 32, Endian.little);
      final offset = data.getUint32(p + 42, Endian.little);
      if (p + 46 + nameSize + extra + comment > directoryEnd ||
          flags & 1 != 0 ||
          (method != 0 && method != 8) ||
          size > limits.maxPartBytes ||
          compressed == 0xffffffff ||
          offset == 0xffffffff ||
          data.getUint16(p + 34, Endian.little) != 0)
        _bad();
      final name = utf8.decode(bytes.sublist(p + 46, p + 46 + nameSize));
      if (name.isEmpty ||
          name.startsWith('/') ||
          name.contains('\\') ||
          name.contains(':') ||
          name.contains('%') ||
          name.contains('\x00') ||
          name.split('/').any((s) => s == '..' || s == '.') ||
          _entries.containsKey(name))
        _bad();
      total += size;
      if (total > limits.maxExpandedBytes) _bad();
      if (offset + 30 > p ||
          data.getUint32(offset, Endian.little) != 0x04034b50 ||
          data.getUint16(offset + 6, Endian.little) != flags ||
          data.getUint16(offset + 8, Endian.little) != method)
        _bad();
      final localNameSize = data.getUint16(offset + 26, Endian.little);
      final start =
          offset +
          30 +
          localNameSize +
          data.getUint16(offset + 28, Endian.little);
      if (start + compressed > data.getUint32(e + 16, Endian.little) ||
          offset + 30 + localNameSize > bytes.length ||
          utf8.decode(
                bytes.sublist(offset + 30, offset + 30 + localNameSize),
              ) !=
              name)
        _bad();
      _entries[name] = (
        start: start,
        compressed: compressed,
        size: size,
        method: method,
        crc: crc,
      );
      p += 46 + nameSize + extra + comment;
    }
    if (p != directoryEnd) _bad();
  }
  final Uint8List bytes;
  final DocumentLimits limits;
  final _entries =
      <String, ({int start, int compressed, int size, int method, int crc})>{};
  Iterable<String> get names => _entries.keys;
  Uint8List? read(String name) {
    final entry = _entries[name];
    if (entry == null) return null;
    final input = Uint8List.sublistView(
      bytes,
      entry.start,
      entry.start + entry.compressed,
    );
    Uint8List result;
    if (entry.method == 0) {
      result = input;
    } else {
      final output = _BoundedSink(entry.size);
      final decoder = ZLibDecoder(raw: true).startChunkedConversion(output);
      for (var p = 0; p < input.length; p += 4096) {
        decoder.add(input.sublist(p, (p + 4096).clamp(0, input.length)));
      }
      decoder.close();
      result = output.builder.takeBytes();
    }
    if (result.length != entry.size || getCrc32(result) != entry.crc) _bad();
    return result;
  }

  Never _bad() => throw const DocumentImportException(
    'unsafe_zip',
    'ZIP 损坏、加密、路径非法或超过解压预算；不支持 ZIP64。',
  );
}

final class _BoundedSink extends ByteConversionSinkBase {
  _BoundedSink(this.limit);
  final int limit;
  final builder = BytesBuilder(copy: false);
  @override
  void add(List<int> chunk) {
    if (builder.length + chunk.length > limit) {
      throw const DocumentImportException('inflate_limit', '解压数据超过声明大小。');
    }
    builder.add(chunk);
  }

  @override
  void close() {}
}
