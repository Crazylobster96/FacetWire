// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import 'core_content_demo.dart';
import 'display_list.dart';
import 'flow_layout_demo.dart';
import 'media_renderer_demo.dart';
import 'models.dart';

class SpikeApp extends StatefulWidget {
  const SpikeApp({required this.client, this.initialDemoPath, super.key});

  final NativeRuntimeClient client;
  final String? initialDemoPath;

  @override
  State<SpikeApp> createState() => _SpikeAppState();
}

class _SpikeAppState extends State<SpikeApp> {
  @override
  void dispose() {
    unawaited(widget.client.close());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'FacetWire Playground Spike',
      theme: ThemeData(colorSchemeSeed: const Color(0xff4f7ef7)),
      home: CoreContentDemoScreen(initialPath: widget.initialDemoPath),
      routes: {
        '/placeholder': (context) => SpikeScreen(client: widget.client),
        '/flow': (context) => FlowLayoutDemoScreen(client: widget.client),
        '/media': (context) => const MediaRendererDemoScreen(),
      },
    );
  }
}

String? parseDemoPathArguments(List<String> arguments) {
  for (var index = 0; index < arguments.length; index += 1) {
    final argument = arguments[index];
    if (argument.startsWith('--demo=')) {
      final value = argument.substring('--demo='.length).trim();
      return value.isEmpty ? null : value;
    }
    if (argument == '--demo' && index + 1 < arguments.length) {
      final value = arguments[index + 1].trim();
      return value.isEmpty ? null : value;
    }
    if (!argument.startsWith('-') && argument.trim().isNotEmpty) {
      return argument.trim();
    }
  }
  return null;
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
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Placeholder Renderer Compatibility')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text('Runtime: $_snapshot'),
            const SizedBox(height: 12),
            Row(
              children: [
                const ExcludeSemantics(child: Text('不透明度')),
                Expanded(
                  child: MergeSemantics(
                    child: Semantics(
                      label: '不透明度',
                      child: Slider(
                        key: const ValueKey('opacity-slider'),
                        value: _opacity,
                        semanticFormatterCallback: (value) =>
                            '${(value * 100).round()}%',
                        onChanged: (value) => setState(() => _opacity = value),
                        onChangeEnd: (_) => _refresh(),
                      ),
                    ),
                  ),
                ),
                ExcludeSemantics(child: Text('${(_opacity * 100).round()}%')),
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
    canvas.scale(
      size.width / logicalSize.width,
      size.height / logicalSize.height,
    );
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
  Future<String> composeFlowDemo({
    required double width,
    required double height,
    required int contentCase,
    required int pageMode,
  }) async {
    final prefixes = ['level-1', 'level-2', 'level-3'];
    final prefix = prefixes[contentCase];
    final fallback = contentCase == 2;
    final virtualPages = pageMode == 1;
    final columns = pageMode == 2;
    final paginated = virtualPages || columns;
    final pageHeight = virtualPages ? 240.0 : (columns ? 300.0 : height);
    final pageCount = virtualPages ? (fallback ? 2 : 3) : 1;
    final columnCount = columns ? 2 : 1;
    final columnGap = columns ? 24.0 : 0.0;
    final contentWidth = width - 48.0;
    final columnWidth =
        (contentWidth - (columnCount - 1) * columnGap) / columnCount;
    return jsonEncode({
      'pluginId': 'org.facetwire.reference.flow-layout',
      'capability': 'facetwire.layout.flow',
      'interfaceVersion': 1,
      'demoCase': contentCase + (pageMode * 3),
      'contentCase': contentCase,
      'pageMode': pageMode,
      'composeStatus': 0,
      'complete': true,
      'pageCount': pageCount,
      'fragmentCount': 3,
      'textFragmentCount': 2,
      'objectFragmentCount': 1,
      'continuousExtent': {
        'width': width,
        'height': paginated ? pageHeight * pageCount : 384.0,
      },
      'pageSize': {'width': width, 'height': pageHeight},
      'pageGap': 0.0,
      'columnCount': columnCount,
      'columnGap': columnGap,
      'contentBounds': {
        'x': 24.0,
        'y': 24.0,
        'width': contentWidth,
        'height': pageHeight - 48.0,
      },
      'planKey': 'demo0000000000000000000000000001',
      'pagesBalanced': true,
      'supportedSlice': 'continuous+virtual-pages+columns+block',
      'nativeRuntime': false,
      'fragments': <Object?>[
        {
          'kind': 'text',
          'sourceItemId': 'paragraph.intro.$prefix',
          'contentKind': '',
          'pageIndex': 0,
          'columnIndex': 0,
          'bounds': {
            'x': 24.0,
            'y': 32.0,
            'width': columns ? columnWidth : contentWidth,
            'height': 56.0,
          },
          'textStart': 0,
          'textEnd': 48,
        },
        {
          'kind': fallback ? 'placeholder' : 'object',
          'sourceItemId': fallback
              ? 'object.missing.$prefix'
              : 'image.hero.$prefix',
          'contentKind': fallback ? 'unknown' : 'image',
          'pageIndex': virtualPages && !fallback ? 1 : 0,
          'columnIndex': 0,
          'bounds': {
            'x': 24.0,
            'y': virtualPages && !fallback ? 40.0 : 104.0,
            'width': fallback ? 180.0 : 240.0 - 20 * contentCase,
            'height': fallback ? 112.0 : 150.0 - 15 * contentCase,
          },
          'textStart': 0,
          'textEnd': 0,
        },
        {
          'kind': 'text',
          'sourceItemId': 'paragraph.closing.$prefix',
          'contentKind': '',
          'pageIndex': virtualPages ? (fallback ? 1 : 2) : 0,
          'columnIndex': columns ? 1 : 0,
          'bounds': {
            'x': columns ? 24.0 + columnWidth + columnGap : 24.0,
            'y': paginated
                ? 34.0
                : fallback
                ? 228.0
                : 266.0 - 15 * contentCase,
            'width': columns ? columnWidth : contentWidth,
            'height': 56.0,
          },
          'textStart': 0,
          'textEnd': 48,
        },
      ],
    });
  }

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
