// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

const _chartAssetId =
    'package:facetwire_placeholder_demo/facetwire_placeholder_demo_bridge.dart';

enum ChartDemoKind {
  bar,
  line,
  pie,
  horizontalBar,
  stackedBar,
  percentBar,
  area,
  stackedArea,
  scatter,
  bubble,
  donut,
  radar,
  heatmap,
  gauge,
  boxPlot,
  histogram,
  waterfall,
  funnel,
  candlestick,
  timeSeries,
  combo,
  divergingBar,
  facetLine,
  rangeArea,
  densityHeatmap,
  wordCloud,
  rose,
  treemap,
  sunburst,
  packedBubble,
}

enum ChartTheme { auto, light, dark, business, academic, highContrast }

enum ChartLegendPlacement { auto, bottom, right, hidden }

enum ChartLabelPolicy { auto, all, important, none }

final class ChartCommand {
  const ChartCommand({
    required this.type,
    required this.values,
    required this.color,
    required this.seriesId,
    required this.categoryId,
    required this.text,
    required this.elementId,
    required this.zIndex,
    required this.promoted,
  });

  factory ChartCommand.fromJson(Map<String, Object?> json) => ChartCommand(
    type: json['type']! as String,
    values: (json['v']! as List<Object?>)
        .map((value) => (value! as num).toDouble())
        .toList(growable: false),
    color: (json['color']! as List<Object?>)
        .map((value) => (value! as num).toDouble())
        .toList(growable: false),
    seriesId: json['seriesId']! as String,
    categoryId: json['categoryId']! as String,
    text: json['text']! as String,
    elementId: json['elementId']! as String,
    zIndex: (json['zIndex']! as num).toInt(),
    promoted: json['promoted']! as bool,
  );

  final String type;
  final List<double> values;
  final List<double> color;
  final String seriesId;
  final String categoryId;
  final String text;
  final String elementId;
  final int zIndex;
  final bool promoted;
}

final class ChartElementRecord {
  const ChartElementRecord({
    required this.index,
    required this.id,
    required this.role,
    required this.label,
    required this.bounds,
    required this.zIndex,
    required this.capabilities,
    required this.flags,
  });

  factory ChartElementRecord.fromJson(Map<String, Object?> json) =>
      ChartElementRecord(
        index: (json['index']! as num).toInt(),
        id: json['id']! as String,
        role: (json['role']! as num).toInt(),
        label: json['label']! as String,
        bounds: (json['bounds']! as List<Object?>)
            .map((value) => (value! as num).toDouble())
            .toList(growable: false),
        zIndex: (json['zIndex']! as num).toInt(),
        capabilities: (json['capabilities']! as num).toInt(),
        flags: (json['flags']! as num).toInt(),
      );

  final int index;
  final String id;
  final int role;
  final String label;
  final List<double> bounds;
  final int zIndex;
  final int capabilities;
  final int flags;
}

final class ChartElementAdjustment {
  const ChartElementAdjustment({
    required this.elementIndex,
    this.opacity = 1,
    this.translateX = 0,
    this.translateY = 0,
    this.scale = 1,
    this.rotationRadians = 0,
    this.promoted = false,
    this.accentColor = false,
  });

  final int elementIndex;
  final double opacity;
  final double translateX;
  final double translateY;
  final double scale;
  final double rotationRadians;
  final bool promoted;
  final bool accentColor;
}

final class ChartTransformReport {
  const ChartTransformReport({
    required this.rotation,
    required this.clip,
    required this.x,
    required this.y,
    required this.width,
    required this.height,
  });

  factory ChartTransformReport.fromJson(Map<String, Object?> json) {
    final destination = json['destination']! as Map<String, Object?>;
    return ChartTransformReport(
      rotation: (json['rotation']! as num).toInt(),
      clip: json['clip']! as bool,
      x: (destination['x']! as num).toDouble(),
      y: (destination['y']! as num).toDouble(),
      width: (destination['width']! as num).toDouble(),
      height: (destination['height']! as num).toDouble(),
    );
  }

  final int rotation;
  final bool clip;
  final double x;
  final double y;
  final double width;
  final double height;
}

final class ChartRenderReport {
  const ChartRenderReport({
    required this.pluginId,
    required this.capability,
    required this.nativeRuntime,
    required this.kind,
    required this.opacity,
    required this.commandsBalanced,
    required this.renderedSeries,
    required this.renderedValues,
    required this.semanticRole,
    required this.uncoveredIsTransparent,
    required this.transform,
    required this.commands,
    required this.elements,
    required this.selectedElementIndex,
  });

  factory ChartRenderReport.fromJson(Map<String, Object?> json) =>
      ChartRenderReport(
        pluginId: json['pluginId']! as String,
        capability: json['capability']! as String,
        nativeRuntime: json['nativeRuntime']! as bool,
        kind: json['kind']! as String,
        opacity: (json['opacity']! as num).toDouble(),
        commandsBalanced: json['commandsBalanced']! as bool,
        renderedSeries: (json['renderedSeries']! as num).toInt(),
        renderedValues: (json['renderedValues']! as num).toInt(),
        semanticRole: (json['semanticRole']! as num).toInt(),
        uncoveredIsTransparent: json['uncoveredIsTransparent']! as bool,
        transform: ChartTransformReport.fromJson(
          json['transform']! as Map<String, Object?>,
        ),
        commands: (json['commands']! as List<Object?>)
            .map(
              (value) => ChartCommand.fromJson(value! as Map<String, Object?>),
            )
            .toList(growable: false),
        elements: (json['elements']! as List<Object?>)
            .map(
              (value) =>
                  ChartElementRecord.fromJson(value! as Map<String, Object?>),
            )
            .toList(growable: false),
        selectedElementIndex:
            (json['selectedElementIndex']! as num).toInt() == 0xffffffff
            ? null
            : (json['selectedElementIndex']! as num).toInt(),
      );

  final String pluginId;
  final String capability;
  final bool nativeRuntime;
  final String kind;
  final double opacity;
  final bool commandsBalanced;
  final int renderedSeries;
  final int renderedValues;
  final int semanticRole;
  final bool uncoveredIsTransparent;
  final ChartTransformReport transform;
  final List<ChartCommand> commands;
  final List<ChartElementRecord> elements;
  final int? selectedElementIndex;
}

abstract interface class ChartRuntimeClient {
  Future<ChartRenderReport> render({
    required double width,
    required double height,
    required ChartDemoKind kind,
    required int rotation,
    required double opacity,
    ChartElementAdjustment? adjustment,
    ChartTheme theme = ChartTheme.business,
    ChartLegendPlacement legend = ChartLegendPlacement.auto,
    ChartLabelPolicy labels = ChartLabelPolicy.auto,
    bool autoLayout = true,
  });

  Future<void> close();
}

final class _ChartBuffer extends Struct {
  external Pointer<Uint8> data;

  @Uint64()
  external int length;
}

typedef _ChartCreate = Int32 Function(Pointer<Pointer<Void>>);
typedef _ChartDestroy = Void Function(Pointer<Void>);
typedef _ChartRenderPresentation = Int32 Function(
  Pointer<Void>,
  Float,
  Float,
  Uint32,
  Uint32,
  Float,
  Uint32,
  Uint32,
  Uint32,
  Uint32,
  Pointer<_ChartBuffer>,
);
typedef _ChartRenderPresentationElements = Int32 Function(
  Pointer<Void>,
  Float,
  Float,
  Uint32,
  Uint32,
  Float,
  Uint32,
  Float,
  Float,
  Float,
  Float,
  Float,
  Uint32,
  Uint32,
  Uint32,
  Uint32,
  Uint32,
  Uint32,
  Pointer<_ChartBuffer>,
);
typedef _ChartRelease = Void Function(Pointer<_ChartBuffer>);

@Native<_ChartCreate>(symbol: 'fwchart_context_create', assetId: _chartAssetId)
external int _chartCreate(Pointer<Pointer<Void>> output);
@Native<_ChartDestroy>(
  symbol: 'fwchart_context_destroy',
  assetId: _chartAssetId,
)
external void _chartDestroy(Pointer<Void> context);
@Native<_ChartRenderPresentation>(
  symbol: 'fwchart_render_presentation_demo',
  assetId: _chartAssetId,
)
external int _chartRenderPresentation(
  Pointer<Void> context,
  double width,
  double height,
  int kind,
  int rotation,
  double opacity,
  int theme,
  int legend,
  int labels,
  int autoLayout,
  Pointer<_ChartBuffer> output,
);
@Native<_ChartRenderPresentationElements>(
  symbol: 'fwchart_render_presentation_elements_demo',
  assetId: _chartAssetId,
)
external int _chartRenderPresentationElements(
  Pointer<Void> context,
  double width,
  double height,
  int kind,
  int rotation,
  double opacity,
  int selectedElementIndex,
  double elementOpacity,
  double translateX,
  double translateY,
  double uniformScale,
  double elementRotationRadians,
  int promoted,
  int accentColor,
  int theme,
  int legend,
  int labels,
  int autoLayout,
  Pointer<_ChartBuffer> output,
);
@Native<_ChartRelease>(symbol: 'fwchart_buffer_release', assetId: _chartAssetId)
external void _chartRelease(Pointer<_ChartBuffer> buffer);

final class NativeAssetChartRuntimeClient implements ChartRuntimeClient {
  NativeAssetChartRuntimeClient._(this._context);

  static NativeAssetChartRuntimeClient open() {
    final output = calloc<Pointer<Void>>();
    try {
      _checkChart(_chartCreate(output), 'fwchart_context_create');
      if (output.value.address == 0) {
        throw StateError('fwchart_context_create returned a null context.');
      }
      return NativeAssetChartRuntimeClient._(output.value);
    } finally {
      calloc.free(output);
    }
  }

  final Pointer<Void> _context;
  bool _closed = false;

  @override
  Future<ChartRenderReport> render({
    required double width,
    required double height,
    required ChartDemoKind kind,
    required int rotation,
    required double opacity,
    ChartElementAdjustment? adjustment,
    ChartTheme theme = ChartTheme.business,
    ChartLegendPlacement legend = ChartLegendPlacement.auto,
    ChartLabelPolicy labels = ChartLabelPolicy.auto,
    bool autoLayout = true,
  }) async {
    if (_closed) throw StateError('Chart client is closed.');
    final output = calloc<_ChartBuffer>();
    try {
      if (adjustment == null) {
        _checkChart(
          _chartRenderPresentation(
            _context,
            width,
            height,
            kind.index,
            rotation,
            opacity,
            theme.index,
            legend.index,
            labels.index,
            autoLayout ? 1 : 0,
            output,
          ),
          'fwchart_render_presentation_demo',
        );
      } else {
        _checkChart(
          _chartRenderPresentationElements(
            _context,
            width,
            height,
            kind.index,
            rotation,
            opacity,
            adjustment.elementIndex,
            adjustment.opacity,
            adjustment.translateX,
            adjustment.translateY,
            adjustment.scale,
            adjustment.rotationRadians,
            adjustment.promoted ? 1 : 0,
            adjustment.accentColor ? 1 : 0,
            theme.index,
            legend.index,
            labels.index,
            autoLayout ? 1 : 0,
            output,
          ),
          'fwchart_render_presentation_elements_demo',
        );
      }
      final json = output.ref.length == 0
          ? ''
          : utf8.decode(output.ref.data.asTypedList(output.ref.length));
      return ChartRenderReport.fromJson(
        jsonDecode(json) as Map<String, Object?>,
      );
    } finally {
      _chartRelease(output);
      calloc.free(output);
    }
  }

  @override
  Future<void> close() async {
    if (_closed) return;
    _chartDestroy(_context);
    _closed = true;
  }
}

final class DemoChartRuntimeClient implements ChartRuntimeClient {
  const DemoChartRuntimeClient();

  @override
  Future<void> close() async {}

  @override
  Future<ChartRenderReport> render({
    required double width,
    required double height,
    required ChartDemoKind kind,
    required int rotation,
    required double opacity,
    ChartElementAdjustment? adjustment,
    ChartTheme theme = ChartTheme.business,
    ChartLegendPlacement legend = ChartLegendPlacement.auto,
    ChartLabelPolicy labels = ChartLabelPolicy.auto,
    bool autoLayout = true,
  }) async => ChartRenderReport.fromJson({
    'pluginId': kind.index >= ChartDemoKind.treemap.index
        ? 'org.facetwire.reference.hierarchical-chart-renderer'
        : 'org.facetwire.reference.core-chart-renderer',
    'capability': kind.index >= ChartDemoKind.treemap.index
        ? 'facetwire.renderer.hierarchical-chart'
        : 'facetwire.renderer.chart',
    'nativeRuntime': false,
    'kind': kind.name,
    'opacity': opacity,
    'commandsBalanced': true,
    'renderedSeries': 1,
    'renderedValues': 1,
    'semanticRole': 5,
    'uncoveredIsTransparent': true,
    'transform': {
      'rotation': rotation,
      'clip': true,
      'destination': {'x': 0, 'y': 0, 'width': width, 'height': height},
    },
    'selectedElementIndex': adjustment?.elementIndex ?? 0xffffffff,
    'elements': [
      {
        'index': 0,
        'id': 'chart/fallback/chart-root',
        'role': 1,
        'label': 'Fallback chart',
        'bounds': [0, 0, 1, 1],
        'zIndex': 0,
        'capabilities': 31,
        'flags': 0,
      },
    ],
    'commands': [
      {
        'type': 'rect',
        'v': [0.2, 0.25, 0.6, 0.55, 0],
        'color': [0.18, 0.45, 0.92, 1],
        'seriesId': 'fallback',
        'categoryId': 'fallback',
        'text': '',
        'elementId': 'chart/fallback/chart-root',
        'zIndex': 0,
        'promoted': adjustment?.promoted ?? false,
      },
    ],
  });
}

void _checkChart(int status, String operation) {
  if (status != 0) {
    throw StateError('$operation failed with Chart bridge status $status.');
  }
}
