// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import 'display_list.dart';
import 'models.dart';

class SpikeApp extends StatelessWidget {
  const SpikeApp({required this.client, super.key});

  final NativeRuntimeClient client;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'FacetWire Playground Spike',
      theme: ThemeData(colorSchemeSeed: const Color(0xff4f7ef7)),
      home: SpikeScreen(client: client),
    );
  }
}

class SpikeScreen extends StatefulWidget {
  const SpikeScreen({required this.client, super.key});

  final NativeRuntimeClient client;

  @override
  State<SpikeScreen> createState() => _SpikeScreenState();
}

class _SpikeScreenState extends State<SpikeScreen> {
  double _opacity = 0.75;
  RenderFrame? _frame;
  String _snapshot = 'loading';
  Object? _error;

  @override
  void initState() {
    super.initState();
    _refresh();
  }

  Future<void> _refresh() async {
    try {
      final snapshot = await widget.client.snapshot();
      final frame = await widget.client.renderPlaceholder(
        width: 640,
        height: 360,
        opacity: _opacity,
      );
      if (mounted) {
        setState(() {
          _snapshot = snapshot;
          _frame = frame;
          _error = null;
        });
      }
    } on Object catch (error) {
      if (mounted) {
        setState(() => _error = error);
      }
    }
  }

  @override
  void dispose() {
    unawaited(widget.client.close());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('FacetWire Playground UI Spike')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text('Runtime: $_snapshot'),
            const SizedBox(height: 12),
            Row(
              children: [
                const Text('不透明度'),
                Expanded(
                  child: Slider(
                    key: const ValueKey('opacity-slider'),
                    value: _opacity,
                    label: '${(_opacity * 100).round()}%',
                    onChanged: (value) => setState(() => _opacity = value),
                    onChangeEnd: (_) => _refresh(),
                  ),
                ),
                Text('${(_opacity * 100).round()}%'),
              ],
            ),
            const SizedBox(height: 12),
            Expanded(child: _buildPreview()),
          ],
        ),
      ),
    );
  }

  Widget _buildPreview() {
    if (_error != null) {
      return Center(child: Text('Native error: $_error'));
    }
    final frame = _frame;
    if (frame == null) {
      return const Center(child: CircularProgressIndicator());
    }
    final commands = decodeDisplayList(frame.displayList);
    final semantics = jsonDecode(frame.semanticsJson) as Map<String, Object?>;
    final nodes = semantics['nodes'] as List<Object?>;
    final firstNode = nodes.first as Map<String, Object?>;
    final label = firstNode['label']! as String;
    return Semantics(
      container: true,
      image: true,
      label: label,
      child: Center(
        child: AspectRatio(
          aspectRatio: 16 / 9,
          child: CustomPaint(
            key: const ValueKey('display-list-canvas'),
            painter: DisplayListPainter(commands),
          ),
        ),
      ),
    );
  }
}

class DisplayListPainter extends CustomPainter {
  const DisplayListPainter(this.commands);

  final List<DisplayCommand> commands;

  @override
  void paint(Canvas canvas, Size size) {
    const logicalSize = Size(640, 360);
    canvas.save();
    canvas.scale(size.width / logicalSize.width, size.height / logicalSize.height);
    for (final command in commands) {
      final paint = Paint()
        ..color = Color.fromRGBO(
          (command.red * 255).round(),
          (command.green * 255).round(),
          (command.blue * 255).round(),
          command.alpha,
        )
        ..style = command.opcode == DisplayOpcode.strokeRoundedRect
            ? PaintingStyle.stroke
            : PaintingStyle.fill
        ..strokeWidth = 2;
      final rect = Rect.fromLTWH(
        command.x,
        command.y,
        command.width,
        command.height,
      );
      switch (command.opcode) {
        case DisplayOpcode.fillRect:
          canvas.drawRect(rect, paint);
          break;
        case DisplayOpcode.fillRoundedRect:
          canvas.drawRRect(
            RRect.fromRectAndRadius(rect, Radius.circular(command.radius)),
            paint,
          );
          break;
        case DisplayOpcode.strokeRoundedRect:
          canvas.drawRRect(
            RRect.fromRectAndRadius(rect, Radius.circular(command.radius)),
            paint,
          );
          break;
      }
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(DisplayListPainter oldDelegate) =>
      !identical(oldDelegate.commands, commands);
}

final class DemoRuntimeClient implements NativeRuntimeClient {
  @override
  Future<void> close() async {}

  @override
  Future<RenderFrame> renderPlaceholder({
    required double width,
    required double height,
    required double opacity,
  }) async {
    final bytes = ByteData(132);
    final view = bytes.buffer.asUint8List();
    view.setAll(0, const [0x46, 0x57, 0x44, 0x4c]);
    bytes.setUint16(4, 1, Endian.little);
    bytes.setUint16(6, 12, Endian.little);
    bytes.setUint32(8, 3, Endian.little);
    _writeCommand(bytes, 12, 1, 0, 0, width, height, 0, .07, .09, .13, opacity);
    _writeCommand(
      bytes,
      52,
      2,
      12,
      12,
      width - 24,
      height - 24,
      12,
      .25,
      .52,
      .96,
      opacity * .3,
    );
    _writeCommand(
      bytes,
      92,
      3,
      12,
      12,
      width - 24,
      height - 24,
      12,
      .56,
      .72,
      1,
      opacity,
    );
    return RenderFrame(
      displayList: view,
      semanticsJson: jsonEncode({
        'revision': 1,
        'nodes': [
          {
            'id': 1,
            'role': 'image',
            'label': 'Unsupported FacetWire zone placeholder',
          },
        ],
      }),
    );
  }

  @override
  Future<String> snapshot() async =>
      '{"abiVersion":1,"renderer":"demo","state":"ready"}';

  static void _writeCommand(
    ByteData data,
    int offset,
    int opcode,
    double x,
    double y,
    double width,
    double height,
    double radius,
    double red,
    double green,
    double blue,
    double alpha,
  ) {
    data.setUint8(offset, opcode);
    final values = [x, y, width, height, radius, red, green, blue, alpha];
    for (var index = 0; index < values.length; index += 1) {
      data.setFloat32(offset + 4 + index * 4, values[index], Endian.little);
    }
  }
}
