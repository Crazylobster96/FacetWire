// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'flow_runtime_client.dart';

const flowLayoutDemoDescriptor =
    'assets/documents/flow-layout-recursive-demo.agscene/'
    'flow-layout-recursive-demo.agscene.dis.json';

final class FlowSceneItem {
  const FlowSceneItem({
    required this.id,
    required this.kind,
    required this.text,
    required this.contentType,
    required this.resourceAsset,
  });

  final String id;
  final String kind;
  final String text;
  final String contentType;
  final String? resourceAsset;
}

final class FlowSceneLevel {
  const FlowSceneLevel({
    required this.documentId,
    required this.title,
    required this.flowId,
    required this.width,
    required this.height,
    required this.canvasSize,
    required this.flowBounds,
    required this.childBounds,
    required this.childClip,
    required this.items,
  });

  final String documentId;
  final String title;
  final String flowId;
  final double width;
  final double height;
  final Size canvasSize;
  final Rect flowBounds;
  final Rect? childBounds;
  final bool childClip;
  final Map<String, FlowSceneItem> items;
}

final class FlowScenePackage {
  const FlowScenePackage(this.levels);
  final List<FlowSceneLevel> levels;
}

abstract interface class FlowScenePackageLoader {
  Future<FlowScenePackage> load([String descriptor = flowLayoutDemoDescriptor]);
}

final class FlowSceneLoader implements FlowScenePackageLoader {
  FlowSceneLoader({AssetBundle? bundle}) : _bundle = bundle ?? rootBundle;

  final AssetBundle _bundle;

  @override
  Future<FlowScenePackage> load([
    String descriptor = flowLayoutDemoDescriptor,
  ]) async {
    final levels = <FlowSceneLevel>[];
    var current = descriptor;
    final visited = <String>{};
    while (true) {
      if (!visited.add(current)) throw FormatException('Scene cycle: $current');
      final decoded = jsonDecode(await _bundle.loadString(current));
      final root = _map(decoded, current);
      if (root['format'] != 'facetwire.agent-scene-package' ||
          root['version'] != '0.1') {
        throw FormatException('Unsupported scene descriptor: $current');
      }
      final directory = current.substring(0, current.lastIndexOf('/'));
      final resources = <String, String>{};
      for (final value in _list(root['resources'], 'resources')) {
        final resource = _map(value, 'resource');
        final id = _string(resource['id'], 'resource.id');
        final asset = '$directory/${_string(resource['source'], '$id.source')}';
        await _bundle.load(asset);
        resources[id] = asset;
      }
      final canvas = _map(root['canvas'], 'canvas');
      final canvasSizeValue = _map(canvas['size'], 'canvas.size');
      final canvasSize = Size(
        _number(canvasSizeValue['width'], 'canvas.size.width'),
        _number(canvasSizeValue['height'], 'canvas.size.height'),
      );
      final pages = _list(canvas['pages'], 'canvas.pages');
      if (pages.isEmpty) throw const FormatException('Canvas has no page');
      final page = _map(pages.first, 'page');
      Map<String, Object?>? flow;
      Rect? flowBounds;
      String? childSource;
      Rect? childBounds;
      var childClip = false;
      for (final layerValue in _list(page['layers'], 'page.layers')) {
        final layer = _map(layerValue, 'layer');
        for (final zoneValue in _list(layer['zones'], 'layer.zones')) {
          final zone = _map(zoneValue, 'zone');
          final content = _map(zone['content'], 'zone.content');
          final bounds = _rect(zone['bounds'], 'zone.bounds');
          if (content['type'] == 'flow') {
            flow = content;
            flowBounds = bounds;
          }
          if (content['type'] == 'document') {
            childSource =
                '$directory/${_string(content['source'], 'document.source')}';
            childBounds = bounds;
            final placement = _map(content['placement'], 'document.placement');
            childClip = placement['clip'] == true;
          }
        }
      }
      if (flow == null) throw FormatException('$current has no Flow zone');
      if (flowBounds == null) {
        throw FormatException('$current Flow zone has no bounds');
      }
      final template = _map(flow['pageTemplate'], 'flow.pageTemplate');
      if (template['mode'] != 'continuous') {
        throw FormatException('$current demo must use continuous mode');
      }
      final size = _map(template['size'], 'flow.pageTemplate.size');
      final items = <String, FlowSceneItem>{};
      for (final value in _list(flow['items'], 'flow.items')) {
        final item = _map(value, 'flow.item');
        final id = _string(item['id'], 'flow.item.id');
        final kind = _string(item['kind'], '$id.kind');
        var text = '';
        var contentType = '';
        String? resourceAsset;
        if (kind == 'paragraph') {
          for (final segmentValue in _list(item['segments'], '$id.segments')) {
            final segment = _map(segmentValue, 'segment');
            if (segment['type'] == 'text') {
              text += _string(segment['text'], '$id.segment.text');
            }
          }
        } else if (kind == 'object') {
          final objectContent = _map(item['content'], '$id.content');
          contentType = _string(objectContent['type'], '$id.content.type');
          final resource = objectContent['resource'];
          if (resource is String) resourceAsset = resources[resource];
        } else {
          throw FormatException('$id has unsupported kind $kind');
        }
        if (items.containsKey(id)) throw FormatException('Duplicate item: $id');
        items[id] = FlowSceneItem(
          id: id,
          kind: kind,
          text: text,
          contentType: contentType,
          resourceAsset: resourceAsset,
        );
      }
      levels.add(
        FlowSceneLevel(
          documentId: _string(root['id'], 'id'),
          title: _string(root['title'], 'title'),
          flowId: _string(flow['id'], 'flow.id'),
          width: _number(size['width'], 'flow.width'),
          height: _number(size['height'], 'flow.height'),
          canvasSize: canvasSize,
          flowBounds: flowBounds,
          childBounds: childBounds,
          childClip: childClip,
          items: Map.unmodifiable(items),
        ),
      );
      if (childSource == null) break;
      if (levels.length >= 16) {
        throw const FormatException('Nesting exceeds 16');
      }
      current = childSource;
    }
    return FlowScenePackage(List.unmodifiable(levels));
  }

  static Map<String, Object?> _map(Object? value, String name) {
    if (value is! Map<String, Object?>) {
      throw FormatException('$name must be an object');
    }
    return value;
  }

  static List<Object?> _list(Object? value, String name) {
    if (value is! List<Object?>) throw FormatException('$name must be a list');
    return value;
  }

  static String _string(Object? value, String name) {
    if (value is! String || value.isEmpty) {
      throw FormatException('$name must be a non-empty string');
    }
    return value;
  }

  static double _number(Object? value, String name) {
    if (value is! num || !value.isFinite) {
      throw FormatException('$name must be finite');
    }
    return value.toDouble();
  }

  static Rect _rect(Object? value, String name) {
    final bounds = _map(value, name);
    return Rect.fromLTWH(
      _number(bounds['x'], '$name.x'),
      _number(bounds['y'], '$name.y'),
      _number(bounds['width'], '$name.width'),
      _number(bounds['height'], '$name.height'),
    );
  }
}

final class FlowPlanFragment {
  const FlowPlanFragment({
    required this.kind,
    required this.sourceItemId,
    required this.contentKind,
    required this.pageIndex,
    required this.bounds,
  });

  final String kind;
  final String sourceItemId;
  final String contentKind;
  final int pageIndex;
  final Rect bounds;

  factory FlowPlanFragment.fromJson(Map<String, Object?> value) {
    final bounds = value['bounds']! as Map<String, Object?>;
    return FlowPlanFragment(
      kind: value['kind']! as String,
      sourceItemId: value['sourceItemId']! as String,
      contentKind: value['contentKind']! as String,
      pageIndex: (value['pageIndex'] as num?)?.toInt() ?? 0,
      bounds: Rect.fromLTWH(
        (bounds['x']! as num).toDouble(),
        (bounds['y']! as num).toDouble(),
        (bounds['width']! as num).toDouble(),
        (bounds['height']! as num).toDouble(),
      ),
    );
  }
}

final class FlowPlanReport {
  const FlowPlanReport({
    required this.pluginId,
    required this.capability,
    required this.composeStatus,
    required this.complete,
    required this.pageCount,
    required this.fragmentCount,
    required this.continuousExtent,
    required this.pageSize,
    required this.pageGap,
    required this.planKey,
    required this.pagesBalanced,
    required this.nativeRuntime,
    required this.supportedSlice,
    required this.fragments,
  });

  final String pluginId;
  final String capability;
  final int composeStatus;
  final bool complete;
  final int pageCount;
  final int fragmentCount;
  final Size continuousExtent;
  final Size pageSize;
  final double pageGap;
  final String planKey;
  final bool pagesBalanced;
  final bool nativeRuntime;
  final String supportedSlice;
  final List<FlowPlanFragment> fragments;

  factory FlowPlanReport.fromJson(String source) {
    final value = jsonDecode(source) as Map<String, Object?>;
    final extent = value['continuousExtent']! as Map<String, Object?>;
    final pageSize = value['pageSize']! as Map<String, Object?>;
    return FlowPlanReport(
      pluginId: value['pluginId']! as String,
      capability: value['capability']! as String,
      composeStatus: (value['composeStatus']! as num).toInt(),
      complete: value['complete']! as bool,
      pageCount: (value['pageCount']! as num).toInt(),
      fragmentCount: (value['fragmentCount']! as num).toInt(),
      continuousExtent: Size(
        (extent['width']! as num).toDouble(),
        (extent['height']! as num).toDouble(),
      ),
      pageSize: Size(
        (pageSize['width']! as num).toDouble(),
        (pageSize['height']! as num).toDouble(),
      ),
      pageGap: (value['pageGap']! as num).toDouble(),
      planKey: value['planKey']! as String,
      pagesBalanced: value['pagesBalanced']! as bool,
      nativeRuntime: value['nativeRuntime'] as bool? ?? false,
      supportedSlice: value['supportedSlice']! as String,
      fragments: List.unmodifiable(
        (value['fragments']! as List<Object?>).map(
          (fragment) =>
              FlowPlanFragment.fromJson(fragment! as Map<String, Object?>),
        ),
      ),
    );
  }
}

enum FlowCanvasMode { fit, actualSize }

enum FlowSceneMode { recursive, single }

class FlowLayoutDemoScreen extends StatefulWidget {
  const FlowLayoutDemoScreen({required this.client, this.loader, super.key});

  final NativeRuntimeClient client;
  final FlowScenePackageLoader? loader;

  @override
  State<FlowLayoutDemoScreen> createState() => _FlowLayoutDemoScreenState();
}

class _FlowLayoutDemoScreenState extends State<FlowLayoutDemoScreen> {
  FlowScenePackage? _package;
  List<FlowPlanReport>? _reports;
  Object? _error;
  var _selectedLevel = 0;
  var _virtualPagesProbe = false;
  var _opacity = 0.9;
  var _sceneMode = FlowSceneMode.recursive;
  final _levelOpacities = <double>[1.0, 0.78, 0.62];
  var _mode = FlowCanvasMode.fit;
  var _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    try {
      final package = await (widget.loader ?? FlowSceneLoader()).load();
      final reports = await _composeAll(package);
      if (!mounted) return;
      setState(() {
        _package = package;
        _reports = reports;
        _error = null;
        _loading = false;
      });
    } on Object catch (error) {
      if (mounted) {
        setState(() {
          _error = error;
          _loading = false;
        });
      }
    }
  }

  Future<FlowPlanReport> _compose(
    FlowScenePackage package,
    int selected,
  ) async {
    final level = package.levels[selected];
    final source = await widget.client.composeFlowDemo(
      width: level.width,
      height: level.height,
      contentCase: selected,
      virtualPages: _virtualPagesProbe,
    );
    return FlowPlanReport.fromJson(source);
  }

  Future<List<FlowPlanReport>> _composeAll(FlowScenePackage package) =>
      Future.wait([
        for (var index = 0; index < package.levels.length; index += 1)
          _compose(package, index),
      ]);

  Future<void> _refresh({int? level, bool? virtualPages}) async {
    final package = _package;
    if (package == null) return;
    final selected = level ?? _selectedLevel;
    setState(() {
      _selectedLevel = selected;
      if (virtualPages != null) _virtualPagesProbe = virtualPages;
      _loading = true;
    });
    try {
      final reports = await _composeAll(package);
      if (mounted) {
        setState(() {
          _reports = reports;
          _error = null;
          _loading = false;
        });
      }
    } on Object catch (error) {
      if (mounted) {
        setState(() {
          _error = error;
          _loading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final package = _package;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Flow Layout 0.1 验证'),
        bottom: const PreferredSize(
          preferredSize: Size.fromHeight(24),
          child: Padding(
            padding: EdgeInsets.only(bottom: 6),
            child: Text(
              '三层递归场景 · Native Layout Plan · continuous / virtual-pages + block',
            ),
          ),
        ),
      ),
      body: Column(
        children: [
          if (_loading) const LinearProgressIndicator(),
          Expanded(
            child: package == null
                ? Center(
                    child: _error == null
                        ? const CircularProgressIndicator()
                        : SelectableText('Flow demo error: $_error'),
                  )
                : LayoutBuilder(
                    builder: (context, constraints) {
                      final preview = _buildPreview(package);
                      final controls = _buildControls(package);
                      if (constraints.maxWidth >= 900) {
                        return Row(
                          children: [
                            Expanded(child: preview),
                            SizedBox(width: 350, child: controls),
                          ],
                        );
                      }
                      return Column(
                        children: [
                          Expanded(child: preview),
                          SizedBox(height: 310, child: controls),
                        ],
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }

  Widget _buildPreview(FlowScenePackage package) {
    final level = package.levels[_selectedLevel];
    final reports = _reports;
    if (_error != null) return Center(child: SelectableText('$_error'));
    if (reports == null) {
      return const Center(child: CircularProgressIndicator());
    }
    final report = reports[_selectedLevel];
    if (!report.complete) {
      return Center(
        key: const ValueKey('flow-incomplete-result'),
        child: Card(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.info_outline, size: 44),
                const SizedBox(height: 12),
                Text('composeStatus = ${report.composeStatus}'),
                const Text('Layout Plan 未完成，当前结果不会作为完整页面发布。'),
              ],
            ),
          ),
        ),
      );
    }
    if (_sceneMode == FlowSceneMode.recursive) {
      return _buildRecursivePreview(package, reports);
    }
    final canvasWidth = _virtualPagesProbe
        ? report.pageSize.width
        : level.width;
    final canvasHeight = _virtualPagesProbe
        ? report.continuousExtent.height
        : level.height;
    final canvas = Opacity(
      opacity: _opacity,
      child: SizedBox(
        key: const ValueKey('flow-logical-canvas'),
        width: canvasWidth,
        height: canvasHeight,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: _virtualPagesProbe
                ? const Color(0xffe8e9ef)
                : const Color(0xfff8f9ff),
            border: Border.all(color: const Color(0xff6574a8), width: 2),
          ),
          child: Stack(
            clipBehavior: Clip.hardEdge,
            children: [
              if (_virtualPagesProbe)
                for (
                  var pageIndex = 0;
                  pageIndex < report.pageCount;
                  pageIndex += 1
                )
                  Positioned(
                    key: ValueKey('flow-page-$pageIndex'),
                    left: 0,
                    top: pageIndex * (report.pageSize.height + report.pageGap),
                    width: report.pageSize.width,
                    height: report.pageSize.height,
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: const Color(0xfff8f9ff),
                        border: Border.all(color: const Color(0xff9aa5c8)),
                      ),
                    ),
                  ),
              for (final fragment in report.fragments)
                _FlowFragmentView(
                  fragment: fragment,
                  item: level.items[fragment.sourceItemId],
                  topOffset: _virtualPagesProbe
                      ? fragment.pageIndex *
                            (report.pageSize.height + report.pageGap)
                      : 0,
                ),
            ],
          ),
        ),
      ),
    );
    return ColoredBox(
      color: Theme.of(context).colorScheme.surfaceContainerLowest,
      child: _mode == FlowCanvasMode.actualSize
          ? InteractiveViewer(
              key: const ValueKey('flow-actual-size'),
              constrained: false,
              minScale: 0.25,
              maxScale: 4,
              child: Padding(padding: const EdgeInsets.all(24), child: canvas),
            )
          : Center(
              child: Padding(
                padding: const EdgeInsets.all(20),
                child: FittedBox(
                  key: const ValueKey('flow-fit-viewport'),
                  fit: BoxFit.contain,
                  child: canvas,
                ),
              ),
            ),
    );
  }

  Widget _buildRecursivePreview(
    FlowScenePackage package,
    List<FlowPlanReport> reports,
  ) {
    final origins = <Offset>[];
    var origin = Offset.zero;
    var extent = Size.zero;
    for (var index = 0; index < package.levels.length; index += 1) {
      final level = package.levels[index];
      origins.add(origin);
      extent = Size(
        math.max(extent.width, origin.dx + level.canvasSize.width),
        math.max(extent.height, origin.dy + level.canvasSize.height),
      );
      final child = level.childBounds;
      if (child != null) origin += child.topLeft;
    }
    const colors = <Color>[
      Color(0xffeef3ff),
      Color(0xffeffaf2),
      Color(0xfffff4e8),
    ];
    final canvas = Opacity(
      opacity: _opacity,
      child: SizedBox(
        key: const ValueKey('flow-recursive-canvas'),
        width: extent.width,
        height: extent.height,
        child: Stack(
          clipBehavior: Clip.none,
          children: [
            for (var index = 0; index < package.levels.length; index += 1)
              Positioned(
                left: origins[index].dx,
                top: origins[index].dy,
                width: package.levels[index].canvasSize.width,
                height: package.levels[index].canvasSize.height,
                child: Opacity(
                  key: ValueKey('flow-level-surface-$index'),
                  opacity: _levelOpacities[index],
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: colors[index % colors.length],
                      border: Border.all(
                        color: _selectedLevel == index
                            ? const Color(0xff315bd6)
                            : const Color(0xff7582a8),
                        width: _selectedLevel == index ? 3 : 1.5,
                      ),
                    ),
                    child: Stack(
                      clipBehavior: Clip.none,
                      children: [
                        Positioned(
                          left: package.levels[index].flowBounds.left,
                          top: package.levels[index].flowBounds.top,
                          child: _FlowPlanSurface(
                            levelIndex: index,
                            level: package.levels[index],
                            report: reports[index],
                            virtualPages: _virtualPagesProbe,
                          ),
                        ),
                        if (package.levels[index].childBounds != null)
                          Positioned.fromRect(
                            rect: package.levels[index].childBounds!,
                            child: IgnorePointer(
                              child: DecoratedBox(
                                key: ValueKey('flow-child-zone-$index'),
                                decoration: BoxDecoration(
                                  border: Border.all(
                                    color: const Color(0xff8a4f08),
                                    width: 2,
                                  ),
                                ),
                              ),
                            ),
                          ),
                      ],
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
    return _buildViewport(canvas);
  }

  Widget _buildViewport(Widget canvas) => ColoredBox(
    color: Theme.of(context).colorScheme.surfaceContainerLowest,
    child: _mode == FlowCanvasMode.actualSize
        ? InteractiveViewer(
            key: const ValueKey('flow-actual-size'),
            constrained: false,
            minScale: 0.1,
            maxScale: 4,
            child: Padding(padding: const EdgeInsets.all(24), child: canvas),
          )
        : Center(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: FittedBox(
                key: const ValueKey('flow-fit-viewport'),
                fit: BoxFit.contain,
                child: canvas,
              ),
            ),
          ),
  );

  Widget _buildControls(FlowScenePackage package) {
    final reports = _reports;
    final report = reports == null ? null : reports[_selectedLevel];
    return Material(
      color: Theme.of(context).colorScheme.surfaceContainerLow,
      child: ListView(
        key: const ValueKey('flow-layout-controls'),
        padding: const EdgeInsets.all(16),
        children: [
          Text('Scene / 场景', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            children: [
              for (var index = 0; index < package.levels.length; index += 1)
                ChoiceChip(
                  key: ValueKey('flow-level-$index'),
                  label: Text('Level ${index + 1}'),
                  selected: _selectedLevel == index,
                  onSelected: (_) => _refresh(level: index),
                ),
            ],
          ),
          const SizedBox(height: 10),
          SegmentedButton<FlowSceneMode>(
            segments: const [
              ButtonSegment(
                value: FlowSceneMode.recursive,
                label: Text('递归合成'),
              ),
              ButtonSegment(value: FlowSceneMode.single, label: Text('单层检查')),
            ],
            selected: {_sceneMode},
            onSelectionChanged: (value) =>
                setState(() => _sceneMode = value.single),
          ),
          if (_sceneMode == FlowSceneMode.recursive)
            for (var index = 0; index < package.levels.length; index += 1) ...[
              const SizedBox(height: 6),
              Text(
                'L${index + 1} opacity / 不透明度 '
                '${(_levelOpacities[index] * 100).round()}%',
              ),
              Slider(
                key: ValueKey('flow-level-opacity-$index'),
                value: _levelOpacities[index],
                onChanged: (value) =>
                    setState(() => _levelOpacities[index] = value),
              ),
            ],
          SwitchListTile(
            key: const ValueKey('flow-virtual-pages-probe'),
            contentPadding: EdgeInsets.zero,
            title: const Text('使用 virtual-pages'),
            subtitle: const Text('真实分页：对象整体移页，片段使用页内逻辑坐标'),
            value: _virtualPagesProbe,
            onChanged: (value) => _refresh(virtualPages: value),
          ),
          Text('Viewer opacity / 预览不透明度 ${(_opacity * 100).round()}%'),
          Slider(
            key: const ValueKey('flow-preview-opacity'),
            value: _opacity,
            onChanged: (value) => setState(() => _opacity = value),
          ),
          SegmentedButton<FlowCanvasMode>(
            segments: const [
              ButtonSegment(value: FlowCanvasMode.fit, label: Text('随窗口适配')),
              ButtonSegment(
                value: FlowCanvasMode.actualSize,
                label: Text('固定 1:1'),
              ),
            ],
            selected: {_mode},
            onSelectionChanged: (value) => setState(() => _mode = value.single),
          ),
          const SizedBox(height: 16),
          Text(
            'Native contract / 原生合同',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 8),
          if (report != null) ...[
            Wrap(
              spacing: 6,
              runSpacing: 6,
              children: [
                _StatusChip('Native', report.nativeRuntime),
                _StatusChip('Complete', report.complete),
                _StatusChip('Balanced', report.pagesBalanced),
                Chip(label: Text('Status ${report.composeStatus}')),
                Chip(label: Text('${report.fragmentCount} fragments')),
              ],
            ),
            const SizedBox(height: 8),
            SelectableText('${report.capability} · ${report.supportedSlice}'),
            SelectableText('Plan Key ${report.planKey}'),
            SelectableText(package.levels[_selectedLevel].flowId),
            if (!report.nativeRuntime)
              const Text(
                '当前为 Dart 测试回退；设备验收必须显示 Native PASS。',
                key: ValueKey('flow-native-fallback-warning'),
                style: TextStyle(color: Colors.deepOrange),
              ),
          ],
        ],
      ),
    );
  }
}

class _StatusChip extends StatelessWidget {
  const _StatusChip(this.label, this.passed);
  final String label;
  final bool passed;

  @override
  Widget build(BuildContext context) => Chip(
    key: ValueKey('flow-status:${label.toLowerCase()}'),
    avatar: Icon(
      passed ? Icons.check_circle : Icons.cancel,
      size: 18,
      color: passed ? Colors.green : Colors.red,
    ),
    label: Text('$label ${passed ? 'PASS' : 'FAIL'}'),
  );
}

class _FlowPlanSurface extends StatelessWidget {
  const _FlowPlanSurface({
    required this.levelIndex,
    required this.level,
    required this.report,
    required this.virtualPages,
  });

  final int levelIndex;
  final FlowSceneLevel level;
  final FlowPlanReport report;
  final bool virtualPages;

  @override
  Widget build(BuildContext context) {
    final width = virtualPages ? report.pageSize.width : level.width;
    final height = virtualPages ? report.continuousExtent.height : level.height;
    return SizedBox(
      key: ValueKey('flow-level-plan-$levelIndex'),
      width: width,
      height: height,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: virtualPages
              ? const Color(0xffe8e9ef)
              : const Color(0xfff8f9ff),
          border: Border.all(color: const Color(0xff6574a8), width: 2),
        ),
        child: Stack(
          clipBehavior: Clip.hardEdge,
          children: [
            if (virtualPages)
              for (
                var pageIndex = 0;
                pageIndex < report.pageCount;
                pageIndex += 1
              )
                Positioned(
                  key: ValueKey('flow-recursive-page-$levelIndex-$pageIndex'),
                  left: 0,
                  top: pageIndex * (report.pageSize.height + report.pageGap),
                  width: report.pageSize.width,
                  height: report.pageSize.height,
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: const Color(0xfff8f9ff),
                      border: Border.all(color: const Color(0xff9aa5c8)),
                    ),
                  ),
                ),
            for (final fragment in report.fragments)
              _FlowFragmentView(
                fragment: fragment,
                item: level.items[fragment.sourceItemId],
                topOffset: virtualPages
                    ? fragment.pageIndex *
                          (report.pageSize.height + report.pageGap)
                    : 0,
              ),
          ],
        ),
      ),
    );
  }
}

class _FlowFragmentView extends StatelessWidget {
  const _FlowFragmentView({
    required this.fragment,
    required this.item,
    required this.topOffset,
  });

  final FlowPlanFragment fragment;
  final FlowSceneItem? item;
  final double topOffset;

  @override
  Widget build(BuildContext context) {
    final bounds = fragment.bounds;
    return Positioned(
      key: ValueKey(
        'flow-fragment:${fragment.sourceItemId}:page-${fragment.pageIndex}',
      ),
      left: bounds.left,
      top: bounds.top + topOffset,
      width: bounds.width,
      height: bounds.height,
      child: Semantics(
        container: true,
        label: fragment.kind == 'text'
            ? item?.text ?? fragment.sourceItemId
            : 'Flow ${fragment.kind}: ${fragment.sourceItemId}',
        child: _content(context),
      ),
    );
  }

  Widget _content(BuildContext context) {
    if (fragment.kind == 'text') {
      return DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xffe9eeff),
          border: Border.all(color: const Color(0xff98a9e8)),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
          child: Text(item?.text ?? fragment.sourceItemId),
        ),
      );
    }
    if (fragment.kind == 'placeholder') {
      return DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xfffff3dc),
          border: Border.all(color: const Color(0xffd28b16), width: 2),
        ),
        child: const Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.extension_off_outlined),
              Text('Placeholder / 后备占位'),
            ],
          ),
        ),
      );
    }
    final asset = item?.resourceAsset;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xffdfe9ff),
        border: Border.all(color: const Color(0xff426bd7), width: 2),
      ),
      child: asset == null
          ? const Center(child: Icon(Icons.image_not_supported_outlined))
          : Image.asset(asset, fit: BoxFit.cover),
    );
  }
}
