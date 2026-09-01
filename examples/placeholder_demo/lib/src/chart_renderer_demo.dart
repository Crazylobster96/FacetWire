// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import 'chart_runtime_client.dart';

class ChartRendererDemoScreen extends StatefulWidget {
  const ChartRendererDemoScreen({required this.client, super.key});

  final ChartRuntimeClient client;

  @override
  State<ChartRendererDemoScreen> createState() =>
      _ChartRendererDemoScreenState();
}

class _ChartRendererDemoScreenState extends State<ChartRendererDemoScreen> {
  ChartDemoKind _kind = ChartDemoKind.bar;
  int _rotation = 0;
  double _opacity = 0.82;
  ChartTheme _theme = ChartTheme.business;
  ChartLegendPlacement _legend = ChartLegendPlacement.auto;
  ChartLabelPolicy _labels = ChartLabelPolicy.auto;
  bool _autoLayout = true;
  int? _selectedElementIndex;
  double _elementOpacity = 1;
  double _elementTranslateX = 0;
  double _elementTranslateY = 0;
  double _elementScale = 1;
  double _elementRotation = 0;
  bool _elementPromoted = false;
  bool _elementAccent = false;
  ChartRenderReport? _report;
  Object? _error;
  int _token = 0;

  @override
  void initState() {
    super.initState();
    unawaited(_refresh());
  }

  Future<void> _refresh() async {
    final token = ++_token;
    try {
      final report = await widget.client.render(
        width: 640,
        height: 360,
        kind: _kind,
        rotation: _rotation,
        opacity: _opacity,
        theme: _theme,
        legend: _legend,
        labels: _labels,
        autoLayout: _autoLayout,
        adjustment: _selectedElementIndex == null
            ? null
            : ChartElementAdjustment(
                elementIndex: _selectedElementIndex!,
                opacity: _elementOpacity,
                translateX: _elementTranslateX,
                translateY: _elementTranslateY,
                scale: _elementScale,
                rotationRadians: _elementRotation,
                promoted: _elementPromoted,
                accentColor: _elementAccent,
              ),
      );
      if (mounted && token == _token) {
        setState(() {
          _report = report;
          _error = null;
        });
      }
    } on Object catch (error) {
      if (mounted && token == _token) setState(() => _error = error);
    }
  }

  void _change(VoidCallback mutation) {
    setState(mutation);
    unawaited(_refresh());
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Chart Renderer 0.3 / 主题与层级图')),
    body: LayoutBuilder(
      builder: (context, constraints) {
        final compact = constraints.maxWidth < 760;
        final preview = _buildPreview();
        final controls = _buildControls();
        return compact
            ? Column(
                children: [
                  Expanded(child: preview),
                  SizedBox(height: 300, child: controls),
                ],
              )
            : Row(
                children: [
                  Expanded(child: preview),
                  const VerticalDivider(width: 1),
                  SizedBox(width: 360, child: controls),
                ],
              );
      },
    ),
  );

  Widget _buildPreview() {
    final report = _report;
    return ColoredBox(
      color: const Color(0xffeef2f6),
      child: Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: AspectRatio(
            aspectRatio: 16 / 9,
            child: Container(
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                  colors: [Color(0xfffbfcfe), Color(0xfff3f6f9)],
                ),
                border: Border.all(color: const Color(0xffd6dce4)),
                borderRadius: BorderRadius.circular(20),
                boxShadow: const [
                  BoxShadow(
                    color: Color(0x160f172a),
                    blurRadius: 28,
                    offset: Offset(0, 10),
                  ),
                ],
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(19),
                child: _error != null
                    ? Center(child: Text('Chart error\n$_error'))
                    : report == null
                    ? const Center(child: CircularProgressIndicator())
                    : _ChartInteractivePreview(report: report),
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildControls() {
    final report = _report;
    return ListView(
      padding: const EdgeInsets.all(18),
      children: [
        Text(
          'Chart kind / 图表类型',
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          children: [
            for (final kind in ChartDemoKind.values)
              ChoiceChip(
                key: ValueKey('chart-kind-${kind.name}'),
                label: Text(switch (kind) {
                  ChartDemoKind.bar => '柱状图',
                  ChartDemoKind.line => '折线图',
                  ChartDemoKind.pie => '饼图',
                  ChartDemoKind.horizontalBar => '横向柱状',
                  ChartDemoKind.stackedBar => '堆叠柱状',
                  ChartDemoKind.percentBar => '百分比柱状',
                  ChartDemoKind.area => '面积图',
                  ChartDemoKind.stackedArea => '堆叠面积',
                  ChartDemoKind.scatter => '散点图',
                  ChartDemoKind.bubble => '气泡图',
                  ChartDemoKind.donut => '环形图',
                  ChartDemoKind.radar => '雷达图',
                  ChartDemoKind.heatmap => '热力图',
                  ChartDemoKind.gauge => '仪表盘',
                  ChartDemoKind.boxPlot => '箱线图',
                  ChartDemoKind.histogram => '直方图',
                  ChartDemoKind.waterfall => '瀑布图',
                  ChartDemoKind.funnel => '漏斗图',
                  ChartDemoKind.candlestick => 'K 线图',
                  ChartDemoKind.timeSeries => '时间序列',
                  ChartDemoKind.combo => '组合图',
                  ChartDemoKind.divergingBar => '对比柱图',
                  ChartDemoKind.facetLine => '分区折线',
                  ChartDemoKind.rangeArea => '范围面积',
                  ChartDemoKind.densityHeatmap => '密度热力图',
                  ChartDemoKind.wordCloud => '词云',
                  ChartDemoKind.rose => '南丁格尔玫瑰图',
                  ChartDemoKind.treemap => '矩形树图',
                  ChartDemoKind.sunburst => '旭日图',
                  ChartDemoKind.packedBubble => 'Packed Bubble',
                }),
                selected: _kind == kind,
                onSelected: (_) => _change(() {
                  _kind = kind;
                  _resetElementAdjustment();
                }),
              ),
          ],
        ),
        const SizedBox(height: 18),
        Text(
          'Presentation / 主题、自动布局与标签',
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 8),
        DropdownButtonFormField<ChartTheme>(
          key: const ValueKey('chart-theme'),
          initialValue: _theme,
          decoration: const InputDecoration(
            labelText: '主题',
            border: OutlineInputBorder(),
          ),
          items: [
            for (final theme in ChartTheme.values)
              DropdownMenuItem(
                value: theme,
                child: Text(switch (theme) {
                  ChartTheme.auto => '自动',
                  ChartTheme.light => '浅色',
                  ChartTheme.dark => '深色',
                  ChartTheme.business => '商务',
                  ChartTheme.academic => '学术',
                  ChartTheme.highContrast => '高对比度',
                }),
              ),
          ],
          onChanged: (value) {
            if (value != null) _change(() => _theme = value);
          },
        ),
        const SizedBox(height: 10),
        DropdownButtonFormField<ChartLegendPlacement>(
          key: const ValueKey('chart-legend-placement'),
          initialValue: _legend,
          decoration: const InputDecoration(
            labelText: '图例位置',
            border: OutlineInputBorder(),
          ),
          items: [
            for (final placement in ChartLegendPlacement.values)
              DropdownMenuItem(
                value: placement,
                child: Text(switch (placement) {
                  ChartLegendPlacement.auto => '自动',
                  ChartLegendPlacement.bottom => '底部',
                  ChartLegendPlacement.right => '右侧',
                  ChartLegendPlacement.hidden => '隐藏',
                }),
              ),
          ],
          onChanged: (value) {
            if (value != null) _change(() => _legend = value);
          },
        ),
        const SizedBox(height: 10),
        DropdownButtonFormField<ChartLabelPolicy>(
          key: const ValueKey('chart-label-policy'),
          initialValue: _labels,
          decoration: const InputDecoration(
            labelText: '标签策略',
            border: OutlineInputBorder(),
          ),
          items: [
            for (final policy in ChartLabelPolicy.values)
              DropdownMenuItem(
                value: policy,
                child: Text(switch (policy) {
                  ChartLabelPolicy.auto => '自动避让',
                  ChartLabelPolicy.all => '全部',
                  ChartLabelPolicy.important => '仅重要标签',
                  ChartLabelPolicy.none => '隐藏标签',
                }),
              ),
          ],
          onChanged: (value) {
            if (value != null) _change(() => _labels = value);
          },
        ),
        SwitchListTile(
          key: const ValueKey('chart-auto-layout'),
          contentPadding: EdgeInsets.zero,
          title: const Text('自动布局'),
          subtitle: const Text('按画布与标签密度选择步进和图例位置'),
          value: _autoLayout,
          onChanged: (value) => _change(() => _autoLayout = value),
        ),
        const SizedBox(height: 8),
        Text('内容旋转 ${_rotation * 90}°'),
        SegmentedButton<int>(
          segments: const [
            ButtonSegment(value: 0, label: Text('0°')),
            ButtonSegment(value: 1, label: Text('90°')),
            ButtonSegment(value: 2, label: Text('180°')),
            ButtonSegment(value: 3, label: Text('270°')),
          ],
          selected: {_rotation},
          onSelectionChanged: (value) =>
              _change(() => _rotation = value.single),
        ),
        const SizedBox(height: 18),
        Text('不透明度 ${(_opacity * 100).round()}%'),
        Slider(
          key: const ValueKey('chart-opacity'),
          value: _opacity,
          onChanged: (value) => _change(() => _opacity = value),
        ),
        const Divider(height: 30),
        Text(
          'Element Inspector / 元素图层调整',
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 8),
        DropdownButtonFormField<int>(
          key: const ValueKey('chart-element-selector'),
          initialValue: _selectedElementIndex ?? -1,
          isExpanded: true,
          decoration: const InputDecoration(
            labelText: '逻辑元素',
            border: OutlineInputBorder(),
          ),
          items: [
            const DropdownMenuItem(value: -1, child: Text('未选择 / 整体图表')),
            if (report != null)
              for (final element in report.elements)
                DropdownMenuItem(
                  value: element.index,
                  child: Text(
                    '${_elementRoleName(element.role)} · ${element.label}',
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
          ],
          onChanged: (value) => _change(() {
            _resetElementAdjustment();
            _selectedElementIndex = value == -1 ? null : value;
          }),
        ),
        if (_selectedElementIndex != null) ...[
          const SizedBox(height: 12),
          SelectableText(
            report?.elements
                    .where((element) => element.index == _selectedElementIndex)
                    .firstOrNull
                    ?.id ??
                'element $_selectedElementIndex',
            key: const ValueKey('chart-selected-element-id'),
            style: Theme.of(context).textTheme.bodySmall,
          ),
          Text('元素不透明度 ${(_elementOpacity * 100).round()}%'),
          Slider(
            key: const ValueKey('chart-element-opacity'),
            value: _elementOpacity,
            onChanged: (value) => _change(() => _elementOpacity = value),
          ),
          Text('X 平移 ${_elementTranslateX.toStringAsFixed(2)}'),
          Slider(
            key: const ValueKey('chart-element-translate-x'),
            value: _elementTranslateX,
            min: -0.25,
            max: 0.25,
            onChanged: (value) => _change(() => _elementTranslateX = value),
          ),
          Text('Y 平移 ${_elementTranslateY.toStringAsFixed(2)}'),
          Slider(
            key: const ValueKey('chart-element-translate-y'),
            value: _elementTranslateY,
            min: -0.25,
            max: 0.25,
            onChanged: (value) => _change(() => _elementTranslateY = value),
          ),
          Text('缩放 ${_elementScale.toStringAsFixed(2)}×'),
          Slider(
            key: const ValueKey('chart-element-scale'),
            value: _elementScale,
            min: 0.5,
            max: 1.8,
            onChanged: (value) => _change(() => _elementScale = value),
          ),
          Text('旋转 ${(_elementRotation * 180 / math.pi).round()}°'),
          Slider(
            key: const ValueKey('chart-element-rotation'),
            value: _elementRotation,
            min: -math.pi,
            max: math.pi,
            onChanged: (value) => _change(() => _elementRotation = value),
          ),
          SwitchListTile(
            key: const ValueKey('chart-element-promoted'),
            contentPadding: EdgeInsets.zero,
            title: const Text('提升为独立 Layer'),
            subtitle: const Text('增加 zOffset，并保留 Chart 父坐标系'),
            value: _elementPromoted,
            onChanged: (value) => _change(() => _elementPromoted = value),
          ),
          SwitchListTile(
            key: const ValueKey('chart-element-accent'),
            contentPadding: EdgeInsets.zero,
            title: const Text('强调色覆盖'),
            value: _elementAccent,
            onChanged: (value) => _change(() => _elementAccent = value),
          ),
          OutlinedButton.icon(
            key: const ValueKey('chart-element-reset'),
            onPressed: () => _change(_resetElementValues),
            icon: const Icon(Icons.restart_alt),
            label: const Text('重置当前元素展示'),
          ),
        ],
        const Divider(height: 30),
        if (report != null) ...[
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              Chip(
                avatar: Icon(
                  report.nativeRuntime ? Icons.check_circle : Icons.warning,
                  color: report.nativeRuntime ? Colors.green : Colors.orange,
                ),
                label: Text(
                  report.nativeRuntime ? 'Native PASS' : 'Dart fallback',
                ),
              ),
              Chip(label: Text('${report.commands.length} commands')),
              Chip(label: Text('${report.renderedValues} values')),
              Chip(label: Text('${report.elements.length} elements')),
              Chip(
                label: Text(
                  report.commandsBalanced ? 'Balanced PASS' : 'Balanced FAIL',
                ),
              ),
              Chip(
                label: Text(
                  report.uncoveredIsTransparent
                      ? 'Transparent PASS'
                      : 'Transparent FAIL',
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          SelectableText(
            key: const ValueKey('chart-report-summary'),
            '${report.pluginId}\n${report.capability}\n'
            '${report.kind} · ${report.renderedSeries} series · '
            'semantics role ${report.semanticRole}',
          ),
        ],
      ],
    );
  }

  void _resetElementValues() {
    _elementOpacity = 1;
    _elementTranslateX = 0;
    _elementTranslateY = 0;
    _elementScale = 1;
    _elementRotation = 0;
    _elementPromoted = false;
    _elementAccent = false;
  }

  void _resetElementAdjustment() {
    _selectedElementIndex = null;
    _resetElementValues();
  }

  String _elementRoleName(int role) => switch (role) {
    1 => '图表根',
    2 => '绘图区',
    3 => '网格',
    4 => 'X 轴',
    5 => 'Y 轴',
    6 => '标题',
    7 => '分类标签',
    8 => '图例项',
    9 => '系列',
    10 => '数据节点',
    11 => '数值标签',
    12 => '注释',
    13 => '图例容器',
    14 => '图例色标',
    15 => '图例标签',
    16 => '图例数值',
    _ => '任意',
  };
}

final class _ChartInteractivePreview extends StatefulWidget {
  const _ChartInteractivePreview({required this.report});

  final ChartRenderReport report;

  @override
  State<_ChartInteractivePreview> createState() =>
      _ChartInteractivePreviewState();
}

final class _ChartInteractivePreviewState
    extends State<_ChartInteractivePreview> {
  ChartCommand? _hoveredCommand;
  Offset? _tooltipAnchor;

  void _updateSelection(Offset position, Size size) {
    final command = _ChartCommandPainter(widget.report)
        .hitTestCommand(position, size);
    if (command == _hoveredCommand && position == _tooltipAnchor) return;
    setState(() {
      _hoveredCommand = command;
      _tooltipAnchor = command == null ? null : position;
    });
  }

  void _clearSelection() {
    if (_hoveredCommand == null) return;
    setState(() {
      _hoveredCommand = null;
      _tooltipAnchor = null;
    });
  }

  String _tooltipTitle(ChartCommand command) {
    if (command.text.isNotEmpty) return command.text;
    if (command.categoryId.isNotEmpty) return command.categoryId;
    if (command.seriesId.isNotEmpty) return command.seriesId;
    final segments = command.elementId.split('/');
    return segments.isEmpty ? 'Chart element' : segments.last;
  }

  String _tooltipDetails(ChartCommand command) {
    final matchingValue = widget.report.commands.where(
      (candidate) =>
          candidate.type == 'label' &&
          candidate.text.isNotEmpty &&
          candidate.elementId == command.elementId,
    );
    final parts = <String>[
      if (command.seriesId.isNotEmpty) command.seriesId,
      if (command.categoryId.isNotEmpty &&
          command.categoryId != command.seriesId)
        command.categoryId,
      if (matchingValue.isNotEmpty && matchingValue.first.text != command.text)
        matchingValue.first.text,
    ];
    return parts.join('  ·  ');
  }

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final size = Size(constraints.maxWidth, constraints.maxHeight);
      final anchor = _tooltipAnchor;
      final command = _hoveredCommand;
      final tooltipWidth = math.max(0.0, math.min(240.0, size.width - 24));
      final maximumLeft = math.max(12.0, size.width - tooltipWidth - 12);
      final maximumTop = math.max(12.0, size.height - 88);
      final left = anchor == null
          ? 0.0
          : (anchor.dx + 14).clamp(12.0, maximumLeft);
      final top = anchor == null
          ? 0.0
          : (anchor.dy + 14).clamp(12.0, maximumTop);
      return MouseRegion(
        cursor: command == null
            ? SystemMouseCursors.basic
            : SystemMouseCursors.click,
        onHover: (event) => _updateSelection(event.localPosition, size),
        onExit: (_) => _clearSelection(),
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTapDown: (details) => _updateSelection(details.localPosition, size),
          child: Stack(
            clipBehavior: Clip.hardEdge,
            children: [
              Positioned.fill(
                child: ClipRect(
                  key: const ValueKey('native-chart-viewport-clip'),
                  child: CustomPaint(
                    key: const ValueKey('native-chart-preview'),
                    painter: _ChartCommandPainter(widget.report),
                  ),
                ),
              ),
              if (command != null && anchor != null)
                Positioned(
                  left: left,
                  top: top,
                  width: tooltipWidth,
                  child: IgnorePointer(
                    child: ClipRRect(
                      borderRadius: BorderRadius.circular(12),
                      child: BackdropFilter(
                        filter: ui.ImageFilter.blur(sigmaX: 12, sigmaY: 12),
                        child: Container(
                          key: const ValueKey('chart-glass-tooltip'),
                          padding: const EdgeInsets.symmetric(
                            horizontal: 14,
                            vertical: 11,
                          ),
                          decoration: BoxDecoration(
                            color: const Color(0xdd25303d),
                            border: Border.all(color: const Color(0x33ffffff)),
                            borderRadius: BorderRadius.circular(12),
                            boxShadow: const [
                              BoxShadow(
                                color: Color(0x330f172a),
                                blurRadius: 18,
                                offset: Offset(0, 8),
                              ),
                            ],
                          ),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              Text(
                                _tooltipTitle(command),
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: const TextStyle(
                                  color: Color(0xfff8fafc),
                                  fontSize: 13,
                                  fontWeight: FontWeight.w600,
                                ),
                              ),
                              if (_tooltipDetails(command).isNotEmpty) ...[
                                const SizedBox(height: 4),
                                Text(
                                  _tooltipDetails(command),
                                  maxLines: 2,
                                  overflow: TextOverflow.ellipsis,
                                  style: const TextStyle(
                                    color: Color(0xffcbd5e1),
                                    fontSize: 12,
                                    height: 1.25,
                                  ),
                                ),
                              ],
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
      );
    },
  );
}

final class _ChartCommandPainter extends CustomPainter {
  const _ChartCommandPainter(this.report);

  final ChartRenderReport report;

  static const _roundedBarKinds = <String>{
    'bar',
    'horizontal-bar',
    'stacked-bar',
    'percent-bar',
    'histogram',
    'waterfall',
    'diverging-bar',
  };
  static const _smoothLineKinds = <String>{
    'line',
    'time-series',
    'facet-line',
    'combo',
  };

  Offset _point(double x, double y, Size size) {
    final transform = report.transform;
    final dx = transform.x / 640 * size.width;
    final dy = transform.y / 360 * size.height;
    final width = transform.width / 640 * size.width;
    final height = transform.height / 360 * size.height;
    return switch (transform.rotation) {
      1 => Offset(dx + (1 - y) * width, dy + x * height),
      2 => Offset(dx + (1 - x) * width, dy + (1 - y) * height),
      3 => Offset(dx + y * width, dy + (1 - x) * height),
      _ => Offset(dx + x * width, dy + y * height),
    };
  }

  Color _color(ChartCommand command) => Color.fromRGBO(
    (command.color[0] * 255).round(),
    (command.color[1] * 255).round(),
    (command.color[2] * 255).round(),
    (command.color[3] * report.opacity).clamp(0, 1),
  );

  Path _rectPath(ChartCommand command, Size size) {
    final x = command.values[0];
    final y = command.values[1];
    final width = command.values[2];
    final height = command.values[3];
    final points = <Offset>[
      _point(x, y, size),
      _point(x + width, y, size),
      _point(x + width, y + height, size),
      _point(x, y + height, size),
    ];
    return Path()
      ..moveTo(points[0].dx, points[0].dy)
      ..lineTo(points[1].dx, points[1].dy)
      ..lineTo(points[2].dx, points[2].dy)
      ..lineTo(points[3].dx, points[3].dy)
      ..close();
  }

  Rect _rectBounds(ChartCommand command, Size size) =>
      _rectPath(command, size).getBounds();

  Path _polygonPath(ChartCommand command, Size size) {
    final path = Path();
    for (var index = 0; index + 1 < command.values.length; index += 2) {
      final point = _point(
        command.values[index],
        command.values[index + 1],
        size,
      );
      if (index == 0) {
        path.moveTo(point.dx, point.dy);
      } else {
        path.lineTo(point.dx, point.dy);
      }
    }
    return path..close();
  }

  Path _sectorPath(ChartCommand command, Size size) {
    final centerX = command.values[0];
    final centerY = command.values[1];
    final radius = command.values[2];
    final innerRadius = command.values[3];
    final start = command.values[4];
    final sweep = command.values[5];
    final path = Path()..fillType = PathFillType.evenOdd;
    final center = _point(centerX, centerY, size);
    path.moveTo(center.dx, center.dy);
    for (var step = 0; step <= 48; step += 1) {
      final angle = start + sweep * step / 48;
      final point = _point(
        centerX + math.cos(angle) * radius,
        centerY + math.sin(angle) * radius,
        size,
      );
      path.lineTo(point.dx, point.dy);
    }
    path.close();
    if (innerRadius > 0) {
      final inner = Path();
      for (var step = 0; step <= 48; step += 1) {
        final angle = start + sweep * (48 - step) / 48;
        final point = _point(
          centerX + math.cos(angle) * innerRadius,
          centerY + math.sin(angle) * innerRadius,
          size,
        );
        if (step == 0) {
          inner.moveTo(point.dx, point.dy);
        } else {
          inner.lineTo(point.dx, point.dy);
        }
      }
      path.addPath(inner..close(), Offset.zero);
    }
    return path;
  }

  double _distanceToSegment(Offset point, Offset start, Offset end) {
    final delta = end - start;
    final lengthSquared = delta.dx * delta.dx + delta.dy * delta.dy;
    if (lengthSquared == 0) return (point - start).distance;
    final ratio =
        ((point - start).dx * delta.dx + (point - start).dy * delta.dy) /
        lengthSquared;
    final nearest = start + delta * ratio.clamp(0.0, 1.0);
    return (point - nearest).distance;
  }

  bool _contains(ChartCommand command, Offset position, Size size) {
    if (command.color.length >= 4 &&
        command.color[3] * report.opacity <= 0.02) {
      return false;
    }
    return switch (command.type) {
      'rect' => _rectPath(command, size).contains(position),
      'circle' =>
        (position - _point(command.values[0], command.values[1], size))
                .distance <=
            command.values[2] * size.shortestSide + 6,
      'sector' => _sectorPath(command, size).contains(position),
      'polygon' =>
        command.values.length >= 6 &&
            _polygonPath(command, size).contains(position),
      'line' =>
        _distanceToSegment(
              position,
              _point(command.values[0], command.values[1], size),
              _point(command.values[2], command.values[3], size),
            ) <=
            math.max(6, command.values[4] * size.shortestSide + 3),
      'label' =>
        (position - _point(command.values[0], command.values[1], size))
                .distance <=
            math.max(12, command.values[2] * size.shortestSide),
      _ => false,
    };
  }

  ChartCommand? hitTestCommand(Offset position, Size size) {
    final indexedCommands =
        List.generate(
          report.commands.length,
          (index) => (index: index, command: report.commands[index]),
        )..sort((left, right) {
          final order = right.command.zIndex.compareTo(left.command.zIndex);
          return order != 0 ? order : right.index.compareTo(left.index);
        });
    for (final entry in indexedCommands) {
      final command = entry.command;
      final isGuide =
          command.type == 'line' &&
          command.seriesId.isEmpty &&
          command.categoryId.isEmpty;
      if (!isGuide && _contains(command, position, size)) return command;
    }
    return null;
  }

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.clipRect(Offset.zero & size);
    final indexedCommands =
        List.generate(
          report.commands.length,
          (index) => (index: index, command: report.commands[index]),
        )..sort((left, right) {
          final order = left.command.zIndex.compareTo(right.command.zIndex);
          return order != 0 ? order : left.index.compareTo(right.index);
        });
    for (final entry in indexedCommands) {
      final command = entry.command;
      final paint = Paint()
        ..color = _color(command)
        ..style = PaintingStyle.fill;
      switch (command.type) {
        case 'rect':
          if (_roundedBarKinds.contains(report.kind) &&
              command.seriesId.isNotEmpty) {
            final rect = _rectBounds(command, size);
            final radius = math.min(6.0, rect.shortestSide * 0.16);
            canvas.drawRRect(
              RRect.fromRectAndRadius(rect, Radius.circular(radius)),
              paint,
            );
          } else {
            canvas.drawPath(_rectPath(command, size), paint);
          }
        case 'line':
          paint
            ..style = PaintingStyle.stroke
            ..strokeWidth = command.values[4] * size.shortestSide
            ..strokeCap = StrokeCap.round
            ..strokeJoin = StrokeJoin.round;
          final start = _point(command.values[0], command.values[1], size);
          final end = _point(command.values[2], command.values[3], size);
          if (_smoothLineKinds.contains(report.kind) &&
              command.seriesId.isNotEmpty) {
            final midpoint = (start.dx + end.dx) / 2;
            canvas.drawPath(
              Path()
                ..moveTo(start.dx, start.dy)
                ..cubicTo(midpoint, start.dy, midpoint, end.dy, end.dx, end.dy),
              paint,
            );
          } else {
            canvas.drawLine(start, end, paint);
          }
        case 'circle':
          final center = _point(command.values[0], command.values[1], size);
          canvas.drawCircle(
            center,
            command.values[2] * size.shortestSide,
            paint,
          );
        case 'sector':
          canvas.drawPath(_sectorPath(command, size), paint);
        case 'polygon':
          if (command.values.length < 6) continue;
          canvas.drawPath(_polygonPath(command, size), paint);
        case 'label':
          final anchor = _point(command.values[0], command.values[1], size);
          final painter = TextPainter(
            text: TextSpan(
              text: command.text,
              style: TextStyle(
                color: _color(command),
                fontSize: math.max(8, command.values[2] * size.shortestSide),
                fontWeight: command.values[2] >= 0.04
                    ? FontWeight.w600
                    : FontWeight.w400,
                letterSpacing: 0.1,
              ),
            ),
            textDirection: ui.TextDirection.ltr,
            maxLines: 1,
          )..layout(maxWidth: size.width * 0.35);
          painter.paint(
            canvas,
            anchor - Offset(painter.width / 2, painter.height / 2),
          );
      }
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant _ChartCommandPainter oldDelegate) =>
      oldDelegate.report != report;
}
