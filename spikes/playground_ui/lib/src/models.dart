// SPDX-License-Identifier: MPL-2.0
import 'dart:typed_data';

enum DisplayOpcode {
  fillRect(1),
  fillRoundedRect(2),
  strokeRoundedRect(3);

  const DisplayOpcode(this.wireValue);
  final int wireValue;

  static DisplayOpcode fromWire(int value) => values.firstWhere(
    (candidate) => candidate.wireValue == value,
    orElse: () => throw FormatException('Unknown display opcode: $value'),
  );
}

final class DisplayCommand {
  const DisplayCommand({
    required this.opcode,
    required this.x,
    required this.y,
    required this.width,
    required this.height,
    required this.radius,
    required this.red,
    required this.green,
    required this.blue,
    required this.alpha,
  });

  final DisplayOpcode opcode;
  final double x;
  final double y;
  final double width;
  final double height;
  final double radius;
  final double red;
  final double green;
  final double blue;
  final double alpha;
}

final class RenderFrame {
  const RenderFrame({required this.displayList, required this.semanticsJson});

  final Uint8List displayList;
  final String semanticsJson;
}

abstract interface class NativeRuntimeClient {
  Future<String> snapshot();

  Future<RenderFrame> renderPlaceholder({
    required double width,
    required double height,
    required double opacity,
  });

  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int demoCase,
  });

  Future<void> close();
}
