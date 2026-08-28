// SPDX-License-Identifier: MPL-2.0
import 'dart:typed_data';

import 'package:facetwire_playground_ui_spike/src/display_list.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('decodes a valid one-command batch', () {
    final data = ByteData(52);
    final bytes = data.buffer.asUint8List();
    bytes.setAll(0, const [0x46, 0x57, 0x44, 0x4c]);
    data.setUint16(4, 1, Endian.little);
    data.setUint16(6, 12, Endian.little);
    data.setUint32(8, 1, Endian.little);
    data.setUint8(12, 1);
    final values = [0.0, 0.0, 100.0, 50.0, 0.0, 0.1, 0.2, 0.3, 0.4];
    for (var index = 0; index < values.length; index += 1) {
      data.setFloat32(16 + index * 4, values[index], Endian.little);
    }

    final commands = decodeDisplayList(bytes);

    expect(commands, hasLength(1));
    expect(commands.single.width, 100);
    expect(commands.single.alpha, closeTo(0.4, 0.0001));
  });

  test('rejects unknown versions, lengths, and non-finite values', () {
    final badVersion = Uint8List.fromList([
      0x46,
      0x57,
      0x44,
      0x4c,
      2,
      0,
      12,
      0,
      0,
      0,
      0,
      0,
    ]);
    expect(() => decodeDisplayList(badVersion), throwsFormatException);

    final badLength = Uint8List.fromList([
      0x46,
      0x57,
      0x44,
      0x4c,
      1,
      0,
      12,
      0,
      1,
      0,
      0,
      0,
    ]);
    expect(() => decodeDisplayList(badLength), throwsFormatException);
  });
}
