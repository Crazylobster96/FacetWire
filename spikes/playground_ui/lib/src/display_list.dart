// SPDX-License-Identifier: MPL-2.0
import 'dart:typed_data';

import 'models.dart';

const int _headerSize = 12;
const int _commandSize = 40;
const int _maxCommandCount = 100000;

List<DisplayCommand> decodeDisplayList(Uint8List bytes) {
  if (bytes.length < _headerSize) {
    throw const FormatException('Display list is shorter than its header.');
  }
  if (bytes[0] != 0x46 ||
      bytes[1] != 0x57 ||
      bytes[2] != 0x44 ||
      bytes[3] != 0x4c) {
    throw const FormatException('Invalid display-list magic.');
  }

  final data = ByteData.sublistView(bytes);
  final version = data.getUint16(4, Endian.little);
  final headerSize = data.getUint16(6, Endian.little);
  final commandCount = data.getUint32(8, Endian.little);
  if (version != 1 || headerSize != _headerSize) {
    throw FormatException(
      'Unsupported display-list header: version=$version size=$headerSize.',
    );
  }
  if (commandCount > _maxCommandCount) {
    throw const FormatException('Display-list command limit exceeded.');
  }
  final expectedLength = headerSize + commandCount * _commandSize;
  if (bytes.length != expectedLength) {
    throw FormatException(
      'Display-list length is ${bytes.length}; expected $expectedLength.',
    );
  }

  return List<DisplayCommand>.generate(commandCount, (index) {
    final offset = headerSize + index * _commandSize;
    if (bytes[offset + 1] != 0 ||
        bytes[offset + 2] != 0 ||
        bytes[offset + 3] != 0) {
      throw const FormatException('Reserved command bytes must be zero.');
    }
    final values = List<double>.generate(
      9,
      (valueIndex) =>
          data.getFloat32(offset + 4 + valueIndex * 4, Endian.little),
      growable: false,
    );
    if (values.any((value) => !value.isFinite)) {
      throw const FormatException('Display command contains non-finite data.');
    }
    if (values[2] < 0 ||
        values[3] < 0 ||
        values[4] < 0 ||
        values.skip(5).any((value) => value < 0 || value > 1)) {
      throw const FormatException('Display command is outside valid ranges.');
    }
    return DisplayCommand(
      opcode: DisplayOpcode.fromWire(bytes[offset]),
      x: values[0],
      y: values[1],
      width: values[2],
      height: values[3],
      radius: values[4],
      red: values[5],
      green: values[6],
      blue: values[7],
      alpha: values[8],
    );
  }, growable: false);
}
