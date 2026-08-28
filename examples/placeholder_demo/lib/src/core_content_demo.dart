// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';
import 'package:path_provider/path_provider.dart';

import 'visual_transform.dart';

const coreContentDemoDescriptor =
    'assets/documents/core-content-overlap-demo.agscene/'
    'core-content-overlap-demo.agscene.dis.json';

const richMediaShowcaseDescriptor =
    'assets/documents/rich-media-showcase.agscene/'
    'rich-media-showcase.agscene.dis.json';

final class DemoBounds {
  const DemoBounds(this.x, this.y, this.width, this.height);

  final double x;
  final double y;
  final double width;
  final double height;
}

enum DemoCanvasScaleMode { fitViewport, actualSize }

final class DemoZone {
  const DemoZone({
    required this.key,
    required this.id,
    required this.bounds,
    required this.content,
    required this.resourceAsset,
    required this.posterAsset,
    required this.child,
    required this.documentFit,
  });

  final String key;
  final String id;
  final DemoBounds bounds;
  final Map<String, Object?> content;
  final String? resourceAsset;
  final String? posterAsset;
  final DemoDocument? child;
  final String documentFit;

  String get type => content['type']! as String;
  double get opacity => ((content['opacity'] as num?) ?? 1).toDouble();
}

final class DemoLayer {
  const DemoLayer({required this.id, required this.z, required this.zones});

  final String id;
  final int z;
  final List<DemoZone> zones;
}

final class DemoDocument {
  const DemoDocument({
    required this.id,
    required this.title,
    required this.descriptorAsset,
    required this.depth,
    required this.width,
    required this.height,
    required this.layers,
  });

  final String id;
  final String title;
  final String descriptorAsset;
  final int depth;
  final double width;
  final double height;
  final List<DemoLayer> layers;

  Iterable<DemoDocument> get documents sync* {
    yield this;
    for (final layer in layers) {
      for (final zone in layer.zones) {
        final child = zone.child;
        if (child != null) yield* child.documents;
      }
    }
  }

  Iterable<DemoZone> get zones sync* {
    for (final layer in layers) {
      for (final zone in layer.zones) {
        yield zone;
        final child = zone.child;
        if (child != null) yield* child.zones;
      }
    }
  }
}

final class CoreContentPackageLoader {
  CoreContentPackageLoader({AssetBundle? bundle})
    : _bundle = bundle ?? rootBundle;

  final AssetBundle _bundle;

  Future<DemoDocument> load(String descriptorAsset) =>
      _load(descriptorAsset, 1, <String>{});

  Future<DemoDocument> loadPath(String path) =>
      _LocalCoreContentPackageLoader().load(path);

  Future<DemoDocument> _load(
    String descriptorAsset,
    int depth,
    Set<String> ancestors,
  ) async {
    if (depth > 16) throw const FormatException('Document nesting exceeds 16');
    if (!ancestors.add(descriptorAsset)) {
      throw FormatException('Document cycle: $descriptorAsset');
    }
    try {
      final decoded = jsonDecode(await _bundle.loadString(descriptorAsset));
      final root = _map(decoded, descriptorAsset);
      if (root['format'] != 'facetwire.agent-scene-package' ||
          root['version'] != '0.1') {
        throw FormatException('Unsupported ASP descriptor: $descriptorAsset');
      }
      final packageRoot = _directory(descriptorAsset);
      final resources = <String, String>{};
      for (final value in _list(root['resources'], 'resources')) {
        final resource = _map(value, 'resource');
        final id = _string(resource['id'], 'resource.id');
        final source = _string(resource['source'], '$id.source');
        final asset = '$packageRoot/$source';
        await _bundle.load(asset);
        resources[id] = asset;
      }
      final canvas = _map(root['canvas'], 'canvas');
      final size = _map(canvas['size'], 'canvas.size');
      final pages = _list(canvas['pages'], 'canvas.pages');
      if (pages.isEmpty) throw const FormatException('Canvas has no pages');
      final page = _map(pages.first, 'canvas.pages[0]');
      final layers = <DemoLayer>[];
      for (final layerValue in _list(page['layers'], 'page.layers')) {
        final layer = _map(layerValue, 'layer');
        final layerId = _string(layer['id'], 'layer.id');
        final zones = <DemoZone>[];
        for (final zoneValue in _list(layer['zones'], '$layerId.zones')) {
          final zone = _map(zoneValue, 'zone');
          final zoneId = _string(zone['id'], 'zone.id');
          final bounds = _map(zone['bounds'], '$zoneId.bounds');
          final content = _map(zone['content'], '$zoneId.content');
          final type = _string(content['type'], '$zoneId.content.type');
          DemoDocument? child;
          String? resourceAsset;
          String? posterAsset;
          var documentFit = 'none';
          if (type == 'document') {
            final source = _string(content['source'], '$zoneId.content.source');
            child = await _load('$packageRoot/$source', depth + 1, ancestors);
            final placement = content['placement'];
            if (placement != null) {
              documentFit =
                  _map(placement, '$zoneId.content.placement')['fit']
                      as String? ??
                  'none';
            }
          } else if (type == 'image' ||
              type == 'animated-image' ||
              type == 'video') {
            final resource = _string(
              content['resource'],
              '$zoneId.content.resource',
            );
            resourceAsset = resources[resource];
            if (resourceAsset == null) {
              throw FormatException('$zoneId references unknown $resource');
            }
            final posterResource = content['posterResource'];
            if (type == 'video' && posterResource != null) {
              final posterId = _string(
                posterResource,
                '$zoneId.content.posterResource',
              );
              posterAsset = resources[posterId];
              if (posterAsset == null) {
                throw FormatException('$zoneId references unknown $posterId');
              }
            }
          } else if (type == 'chart') {
            _validateChart(content, zoneId);
          } else if (type != 'text') {
            throw FormatException('$zoneId uses unsupported demo type $type');
          }
          zones.add(
            DemoZone(
              key: '$descriptorAsset#$zoneId',
              id: zoneId,
              bounds: DemoBounds(
                _number(bounds['x'], '$zoneId.bounds.x'),
                _number(bounds['y'], '$zoneId.bounds.y'),
                _number(bounds['width'], '$zoneId.bounds.width'),
                _number(bounds['height'], '$zoneId.bounds.height'),
              ),
              content: Map<String, Object?>.unmodifiable(content),
              resourceAsset: resourceAsset,
              posterAsset: posterAsset,
              child: child,
              documentFit: documentFit,
            ),
          );
        }
        layers.add(
          DemoLayer(
            id: layerId,
            z: (layer['z'] as num).toInt(),
            zones: List<DemoZone>.unmodifiable(zones),
          ),
        );
      }
      layers.sort((a, b) => a.z.compareTo(b.z));
      return DemoDocument(
        id: _string(root['id'], 'id'),
        title: _string(root['title'], 'title'),
        descriptorAsset: descriptorAsset,
        depth: depth,
        width: _number(size['width'], 'canvas.size.width'),
        height: _number(size['height'], 'canvas.size.height'),
        layers: List<DemoLayer>.unmodifiable(layers),
      );
    } finally {
      ancestors.remove(descriptorAsset);
    }
  }

  static String _directory(String path) =>
      path.substring(0, path.lastIndexOf('/'));

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

  static void _validateChart(Map<String, Object?> content, String zoneId) {
    final data = _list(content['data'], '$zoneId.content.data');
    if (data.isEmpty) {
      throw FormatException('$zoneId chart data must not be empty');
    }
    for (var index = 0; index < data.length; index += 1) {
      final point = _map(data[index], '$zoneId.content.data[$index]');
      _string(point['label'], '$zoneId.content.data[$index].label');
      _number(point['value'], '$zoneId.content.data[$index].value');
    }
  }
}

final class _LocalCoreContentPackageLoader {
  Future<DemoDocument> load(String path) async {
    final requested = path.trim();
    if (requested.isEmpty) {
      throw const FormatException('Package folder or descriptor path is empty');
    }
    final type = await FileSystemEntity.type(requested, followLinks: true);
    final File descriptor;
    final Directory packageRoot;
    if (type == FileSystemEntityType.directory) {
      packageRoot = Directory(requested);
      descriptor = await _findDescriptor(packageRoot);
    } else if (type == FileSystemEntityType.file) {
      descriptor = File(requested);
      packageRoot = descriptor.parent;
    } else {
      throw FormatException('Package path does not exist: $requested');
    }
    final canonicalRoot = Directory(await packageRoot.resolveSymbolicLinks());
    final checkedDescriptor = await _checkedFile(
      descriptor,
      canonicalRoot,
      'descriptor',
    );
    return _load(checkedDescriptor, canonicalRoot, 1, <String>{});
  }

  Future<File> _findDescriptor(Directory directory) async {
    final candidates = <File>[];
    await for (final entity in directory.list(followLinks: false)) {
      if (entity is! File) continue;
      final name = entity.uri.pathSegments.last.toLowerCase();
      if (name.endsWith('.dis.json') || name.endsWith('.dis')) {
        candidates.add(entity);
      }
    }
    if (candidates.isEmpty) {
      throw FormatException(
        'No .dis.json or .dis descriptor in ${directory.path}',
      );
    }
    if (candidates.length == 1) return candidates.single;
    final directoryName = directory.absolute.uri.pathSegments
        .where((segment) => segment.isNotEmpty)
        .last
        .toLowerCase();
    final preferred = candidates.where((candidate) {
      final name = candidate.uri.pathSegments.last.toLowerCase();
      return name == '$directoryName.dis.json' || name == '$directoryName.dis';
    }).toList();
    if (preferred.length == 1) return preferred.single;
    throw FormatException(
      'Package folder has multiple descriptors; specify one directly: '
      '${candidates.map((file) => file.path).join(', ')}',
    );
  }

  Future<DemoDocument> _load(
    File descriptor,
    Directory packageRoot,
    int depth,
    Set<String> ancestors,
  ) async {
    if (depth > 16) throw const FormatException('Document nesting exceeds 16');
    final checkedDescriptor = await _checkedFile(
      descriptor,
      packageRoot,
      'descriptor',
    );
    final identity = checkedDescriptor.path;
    if (!ancestors.add(identity)) {
      throw FormatException('Document cycle: $identity');
    }
    try {
      final decoded = jsonDecode(await checkedDescriptor.readAsString());
      final root = CoreContentPackageLoader._map(decoded, identity);
      if (root['format'] != 'facetwire.agent-scene-package' ||
          root['version'] != '0.1') {
        throw FormatException('Unsupported ASP descriptor: $identity');
      }
      final resources = <String, String>{};
      for (final value in CoreContentPackageLoader._list(
        root['resources'],
        'resources',
      )) {
        final resource = CoreContentPackageLoader._map(value, 'resource');
        final id = CoreContentPackageLoader._string(
          resource['id'],
          'resource.id',
        );
        final source = CoreContentPackageLoader._string(
          resource['source'],
          '$id.source',
        );
        final file = await _checkedFile(
          _resolve(checkedDescriptor, source),
          packageRoot,
          'resource $id',
        );
        resources[id] = file.uri.toString();
      }
      final canvas = CoreContentPackageLoader._map(root['canvas'], 'canvas');
      final size = CoreContentPackageLoader._map(canvas['size'], 'canvas.size');
      final pages = CoreContentPackageLoader._list(
        canvas['pages'],
        'canvas.pages',
      );
      if (pages.isEmpty) throw const FormatException('Canvas has no pages');
      final page = CoreContentPackageLoader._map(
        pages.first,
        'canvas.pages[0]',
      );
      final layers = <DemoLayer>[];
      for (final layerValue in CoreContentPackageLoader._list(
        page['layers'],
        'page.layers',
      )) {
        final layer = CoreContentPackageLoader._map(layerValue, 'layer');
        final layerId = CoreContentPackageLoader._string(
          layer['id'],
          'layer.id',
        );
        final zones = <DemoZone>[];
        for (final zoneValue in CoreContentPackageLoader._list(
          layer['zones'],
          '$layerId.zones',
        )) {
          final zone = CoreContentPackageLoader._map(zoneValue, 'zone');
          final zoneId = CoreContentPackageLoader._string(
            zone['id'],
            'zone.id',
          );
          final bounds = CoreContentPackageLoader._map(
            zone['bounds'],
            '$zoneId.bounds',
          );
          final content = CoreContentPackageLoader._map(
            zone['content'],
            '$zoneId.content',
          );
          final type = CoreContentPackageLoader._string(
            content['type'],
            '$zoneId.content.type',
          );
          DemoDocument? child;
          String? resourceAsset;
          String? posterAsset;
          var documentFit = 'none';
          if (type == 'document') {
            final source = CoreContentPackageLoader._string(
              content['source'],
              '$zoneId.content.source',
            );
            child = await _load(
              _resolve(checkedDescriptor, source),
              packageRoot,
              depth + 1,
              ancestors,
            );
            final placement = content['placement'];
            if (placement != null) {
              documentFit =
                  CoreContentPackageLoader._map(
                        placement,
                        '$zoneId.content.placement',
                      )['fit']
                      as String? ??
                  'none';
            }
          } else if (type == 'image' ||
              type == 'animated-image' ||
              type == 'video') {
            final resource = CoreContentPackageLoader._string(
              content['resource'],
              '$zoneId.content.resource',
            );
            resourceAsset = resources[resource];
            if (resourceAsset == null) {
              throw FormatException('$zoneId references unknown $resource');
            }
            final posterResource = content['posterResource'];
            if (type == 'video' && posterResource != null) {
              final posterId = CoreContentPackageLoader._string(
                posterResource,
                '$zoneId.content.posterResource',
              );
              posterAsset = resources[posterId];
              if (posterAsset == null) {
                throw FormatException('$zoneId references unknown $posterId');
              }
            }
          } else if (type == 'chart') {
            CoreContentPackageLoader._validateChart(content, zoneId);
          } else if (type != 'text') {
            throw FormatException('$zoneId uses unsupported demo type $type');
          }
          zones.add(
            DemoZone(
              key: '$identity#$zoneId',
              id: zoneId,
              bounds: DemoBounds(
                CoreContentPackageLoader._number(
                  bounds['x'],
                  '$zoneId.bounds.x',
                ),
                CoreContentPackageLoader._number(
                  bounds['y'],
                  '$zoneId.bounds.y',
                ),
                CoreContentPackageLoader._number(
                  bounds['width'],
                  '$zoneId.bounds.width',
                ),
                CoreContentPackageLoader._number(
                  bounds['height'],
                  '$zoneId.bounds.height',
                ),
              ),
              content: Map<String, Object?>.unmodifiable(content),
              resourceAsset: resourceAsset,
              posterAsset: posterAsset,
              child: child,
              documentFit: documentFit,
            ),
          );
        }
        layers.add(
          DemoLayer(
            id: layerId,
            z: (layer['z'] as num).toInt(),
            zones: List<DemoZone>.unmodifiable(zones),
          ),
        );
      }
      layers.sort((a, b) => a.z.compareTo(b.z));
      return DemoDocument(
        id: CoreContentPackageLoader._string(root['id'], 'id'),
        title: CoreContentPackageLoader._string(root['title'], 'title'),
        descriptorAsset: identity,
        depth: depth,
        width: CoreContentPackageLoader._number(
          size['width'],
          'canvas.size.width',
        ),
        height: CoreContentPackageLoader._number(
          size['height'],
          'canvas.size.height',
        ),
        layers: List<DemoLayer>.unmodifiable(layers),
      );
    } finally {
      ancestors.remove(identity);
    }
  }

  File _resolve(File descriptor, String relative) => File.fromUri(
    descriptor.parent.uri.resolve(relative.replaceAll('\\', '/')),
  );

  Future<File> _checkedFile(
    File file,
    Directory packageRoot,
    String label,
  ) async {
    if (!await file.exists()) {
      throw FormatException('$label does not exist: ${file.path}');
    }
    final checked = File(await file.resolveSymbolicLinks());
    final root = packageRoot.path;
    final candidate = checked.path;
    final comparisonRoot = Platform.isWindows ? root.toLowerCase() : root;
    final comparisonCandidate = Platform.isWindows
        ? candidate.toLowerCase()
        : candidate;
    if (comparisonCandidate != comparisonRoot &&
        !comparisonCandidate.startsWith(
          '$comparisonRoot${Platform.pathSeparator}',
        )) {
      throw FormatException('$label escapes package root: ${file.path}');
    }
    return checked;
  }
}

class CoreContentDemoScreen extends StatefulWidget {
  const CoreContentDemoScreen({
    this.loader,
    this.descriptorAsset = richMediaShowcaseDescriptor,
    this.initialPath,
    super.key,
  });

  final CoreContentPackageLoader? loader;
  final String descriptorAsset;
  final String? initialPath;

  @override
  State<CoreContentDemoScreen> createState() => _CoreContentDemoScreenState();
}

class _CoreContentDemoScreenState extends State<CoreContentDemoScreen> {
  final Map<String, double> _typeOpacity = {
    'text': 1,
    'image': 1,
    'animated-image': 1,
    'chart': 1,
    'video': 1,
  };
  DemoDocument? _document;
  final Map<String, double> _zoneOpacityOverride = {};
  DemoZone? _selected;
  Object? _error;
  String? _activePath;
  var _loading = false;
  var _scaleMode = DemoCanvasScaleMode.fitViewport;

  @override
  void initState() {
    super.initState();
    _load(widget.initialPath);
  }

  Future<void> _load(String? path) async {
    setState(() => _loading = true);
    try {
      final loader = widget.loader ?? CoreContentPackageLoader();
      final requestedPath = path?.trim();
      final document = requestedPath == null || requestedPath.isEmpty
          ? await loader.load(widget.descriptorAsset)
          : await loader.loadPath(requestedPath);
      if (!mounted) return;
      setState(() {
        _document = document;
        _selected = document.zones.firstWhere((zone) => zone.type == 'text');
        _error = null;
        _activePath = requestedPath == null || requestedPath.isEmpty
            ? null
            : requestedPath;
        _loading = false;
        _zoneOpacityOverride.clear();
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

  Future<void> _showOpenDialog() async {
    final path = await showDialog<String>(
      context: context,
      builder: (dialogContext) => _OpenSourceDialog(initialPath: _activePath),
    );
    if (path != null && mounted) await _load(path);
  }

  double _effectiveOpacity(DemoZone zone) {
    final override = _zoneOpacityOverride[zone.key];
    if (override != null) return override;
    return (zone.opacity * (_typeOpacity[zone.type] ?? 1)).clamp(0, 1);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('FacetWire Rich Media Showcase'),
        actions: [
          IconButton(
            key: const ValueKey('open-demo-source'),
            tooltip: '打开目录或描述文件',
            icon: const Icon(Icons.folder_open_outlined),
            onPressed: _loading ? null : _showOpenDialog,
          ),
          IconButton(
            key: const ValueKey('open-flow-layout-demo'),
            tooltip: 'Flow Layout 0.1 验证',
            icon: const Icon(Icons.view_stream_outlined),
            onPressed: () => Navigator.of(context).pushNamed('/flow'),
          ),
          IconButton(
            key: const ValueKey('open-media-demo'),
            tooltip: 'Audio/Video Renderer 0.1 演示',
            icon: const Icon(Icons.perm_media_outlined),
            onPressed: () => Navigator.of(context).pushNamed('/media'),
          ),
          IconButton(
            key: const ValueKey('open-placeholder-demo'),
            tooltip: 'Placeholder renderer compatibility screen',
            icon: const Icon(Icons.developer_board_outlined),
            onPressed: () => Navigator.of(context).pushNamed('/placeholder'),
          ),
          const SizedBox(width: 8),
        ],
        bottom: const PreferredSize(
          preferredSize: Size.fromHeight(24),
          child: Padding(
            padding: EdgeInsets.only(bottom: 6),
            child: Text('三层原始坐标递归 · Text / Image / GIF / Chart / Video'),
          ),
        ),
      ),
      body: Column(
        children: [
          if (_loading) const LinearProgressIndicator(),
          Expanded(
            child: _error != null && _document == null
                ? Center(child: SelectableText('Demo package error: $_error'))
                : _document == null
                ? const Center(child: CircularProgressIndicator())
                : LayoutBuilder(builder: _buildLoaded),
          ),
        ],
      ),
    );
  }

  Widget _buildLoaded(BuildContext context, BoxConstraints constraints) {
    final preview = _PreviewPane(
      document: _document!,
      selected: _selected,
      opacityFor: _effectiveOpacity,
      onSelect: (zone) => setState(() => _selected = zone),
      scaleMode: _scaleMode,
    );
    final controls = _buildControls();
    if (constraints.maxWidth >= 900) {
      return Row(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Expanded(child: preview),
          SizedBox(width: 330, child: controls),
        ],
      );
    }
    return Column(
      children: [
        Expanded(child: preview),
        SizedBox(height: 250, child: controls),
      ],
    );
  }

  Widget _buildControls() {
    final document = _document!;
    return Material(
      color: Theme.of(context).colorScheme.surfaceContainerLow,
      child: ListView(
        key: const ValueKey('core-content-controls'),
        padding: const EdgeInsets.all(16),
        children: [
          Text(
            'Package / 标准目录包',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 4),
          Text(
            '${document.documents.length} recursive documents · '
            '${document.zones.length} zones',
          ),
          const SizedBox(height: 4),
          SelectableText(
            _activePath ?? 'Built-in: ${document.descriptorAsset}',
            key: const ValueKey('active-demo-source'),
          ),
          if (_error != null) Text('Last load error: $_error'),
          const SizedBox(height: 12),
          Text(
            'Canvas sizing / 画布尺寸',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 6),
          SegmentedButton<DemoCanvasScaleMode>(
            key: const ValueKey('canvas-scale-mode'),
            segments: const [
              ButtonSegment(
                value: DemoCanvasScaleMode.fitViewport,
                label: Text('适应窗口'),
                icon: Icon(Icons.fit_screen_outlined),
              ),
              ButtonSegment(
                value: DemoCanvasScaleMode.actualSize,
                label: Text('固定 1:1'),
                icon: Icon(Icons.center_focus_strong_outlined),
              ),
            ],
            selected: {_scaleMode},
            onSelectionChanged: (selection) =>
                setState(() => _scaleMode = selection.single),
          ),
          const SizedBox(height: 4),
          Text(
            _scaleMode == DemoCanvasScaleMode.fitViewport
                ? '根画布随预览区域等比缩放。'
                : '根画布保持原始逻辑像素，居中显示；窗口较小时可拖动查看。',
            key: const ValueKey('canvas-scale-mode-description'),
          ),
          const SizedBox(height: 12),
          for (final type in const [
            'text',
            'image',
            'animated-image',
            'chart',
            'video',
          ])
            _opacitySlider(type),
          const Divider(),
          Text(
            'Selected layer / 当前层',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 6),
          if (_selected == null)
            const Text('Tap a layer in the canvas.')
          else ...[
            SelectableText(_selected!.id),
            Text('type: ${_selected!.type}'),
            Text('stored opacity: ${_selected!.opacity.toStringAsFixed(2)}'),
            Text(
              'effective opacity: '
              '${_effectiveOpacity(_selected!).toStringAsFixed(2)}',
              key: const ValueKey('selected-effective-opacity'),
            ),
            Slider(
              key: const ValueKey('selected-layer-opacity'),
              value: _effectiveOpacity(_selected!),
              min: 0,
              max: 1,
              divisions: 100,
              label: _effectiveOpacity(_selected!).toStringAsFixed(2),
              semanticFormatterCallback: (value) =>
                  '${(value * 100).round()}% opaque',
              onChanged: (value) =>
                  setState(() => _zoneOpacityOverride[_selected!.key] = value),
            ),
            const Text('当前层会话覆盖：1 完全不透明，0 完全透明。'),
            Text(
              'bounds: ${_selected!.bounds.x.toInt()},'
              '${_selected!.bounds.y.toInt()} · '
              '${_selected!.bounds.width.toInt()}×'
              '${_selected!.bounds.height.toInt()}',
            ),
          ],
          const SizedBox(height: 10),
          const Text(
            '类型滑杆是批量调试系数；当前层滑杆直接覆盖最终 opacity。'
            '所有 opacity 均遵循 1 完全不透明、0 完全透明。'
            '嵌套文档默认 fit=none。',
          ),
        ],
      ),
    );
  }

  Widget _opacitySlider(String type) {
    final value = _typeOpacity[type]!;
    final label = switch (type) {
      'text' => 'Text',
      'image' => 'Image',
      'animated-image' => 'GIF',
      'chart' => 'Chart',
      _ => 'Video',
    };
    return Row(
      children: [
        SizedBox(width: 58, child: Text(label)),
        Expanded(
          child: Slider(
            key: ValueKey('type-opacity:$type'),
            value: value,
            min: 0,
            max: 1,
            divisions: 100,
            label: value.toStringAsFixed(2),
            semanticFormatterCallback: (current) =>
                '${(current * 100).round()}% opaque',
            onChanged: (next) => setState(() => _typeOpacity[type] = next),
          ),
        ),
        SizedBox(
          width: 42,
          child: Text(value.toStringAsFixed(2), textAlign: TextAlign.end),
        ),
      ],
    );
  }
}

class _OpenSourceDialog extends StatefulWidget {
  const _OpenSourceDialog({required this.initialPath});

  final String? initialPath;

  @override
  State<_OpenSourceDialog> createState() => _OpenSourceDialogState();
}

class _OpenSourceDialogState extends State<_OpenSourceDialog> {
  late final TextEditingController _controller;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(text: widget.initialPath ?? '');
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('打开演示目录或描述文件'),
      content: SizedBox(
        width: 620,
        child: TextField(
          key: const ValueKey('demo-source-path'),
          controller: _controller,
          autofocus: true,
          decoration: const InputDecoration(
            labelText: '目录或 .dis.json/.dis 文件路径',
            hintText: r'D:\demos\example.agscene',
            border: OutlineInputBorder(),
          ),
          onSubmitted: (value) => Navigator.pop(context, value.trim()),
        ),
      ),
      actions: [
        TextButton(
          key: const ValueKey('load-builtin-demo'),
          onPressed: () => Navigator.pop(context, ''),
          child: const Text('内置示例'),
        ),
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('取消'),
        ),
        FilledButton(
          key: const ValueKey('load-demo-source'),
          onPressed: () => Navigator.pop(context, _controller.text.trim()),
          child: const Text('打开'),
        ),
      ],
    );
  }
}

final class _PreviewPane extends StatelessWidget {
  const _PreviewPane({
    required this.document,
    required this.selected,
    required this.opacityFor,
    required this.onSelect,
    required this.scaleMode,
  });

  final DemoDocument document;
  final DemoZone? selected;
  final double Function(DemoZone zone) opacityFor;
  final ValueChanged<DemoZone> onSelect;
  final DemoCanvasScaleMode scaleMode;

  @override
  Widget build(BuildContext context) {
    return ColoredBox(
      color: const Color(0xffdfe4ec),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: LayoutBuilder(
          builder: (context, constraints) {
            final fitScale = math.min(
              constraints.maxWidth / document.width,
              constraints.maxHeight / document.height,
            );
            final scale = scaleMode == DemoCanvasScaleMode.fitViewport
                ? fitScale
                : 1.0;
            final isActualSize = scaleMode == DemoCanvasScaleMode.actualSize;
            return InteractiveViewer(
              key: const ValueKey('canvas-interactive-viewer'),
              constrained: false,
              alignment: Alignment.center,
              panEnabled: isActualSize,
              scaleEnabled: !isActualSize,
              minScale: isActualSize ? 1 : 0.5,
              maxScale: isActualSize ? 1 : 5,
              boundaryMargin: const EdgeInsets.all(80),
              child: SizedBox(
                key: const ValueKey('preview-canvas-box'),
                width: document.width * scale,
                height: document.height * scale,
                child: Transform.scale(
                  scale: scale,
                  alignment: Alignment.topLeft,
                  child: SizedBox(
                    width: document.width,
                    height: document.height,
                    child: _DocumentCanvas(
                      document: document,
                      selected: selected,
                      opacityFor: opacityFor,
                      onSelect: onSelect,
                    ),
                  ),
                ),
              ),
            );
          },
        ),
      ),
    );
  }
}

final class _DocumentCanvas extends StatelessWidget {
  const _DocumentCanvas({
    required this.document,
    required this.selected,
    required this.opacityFor,
    required this.onSelect,
  });

  final DemoDocument document;
  final DemoZone? selected;
  final double Function(DemoZone zone) opacityFor;
  final ValueChanged<DemoZone> onSelect;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      key: ValueKey('document-canvas:${document.id}'),
      decoration: BoxDecoration(
        color: Colors.transparent,
        border: Border.all(color: _depthColor(document.depth), width: 2),
      ),
      child: Stack(
        clipBehavior: Clip.hardEdge,
        children: [
          for (final layer in document.layers)
            for (final zone in layer.zones)
              Positioned(
                key: ValueKey(zone.key),
                left: zone.bounds.x,
                top: zone.bounds.y,
                width: zone.bounds.width,
                height: zone.bounds.height,
                child: _ZoneSurface(
                  zone: zone,
                  selected: identical(zone, selected),
                  effectiveOpacity: opacityFor(zone),
                  opacityFor: opacityFor,
                  onSelect: onSelect,
                  selectedZone: selected,
                ),
              ),
          Positioned(
            left: 4,
            top: 4,
            child: IgnorePointer(
              child: DecoratedBox(
                decoration: BoxDecoration(color: _depthColor(document.depth)),
                child: Padding(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 5,
                    vertical: 2,
                  ),
                  child: Text(
                    '${document.depth} · ${document.id}',
                    style: const TextStyle(color: Colors.white, fontSize: 9),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  static Color _depthColor(int depth) => switch (depth) {
    1 => const Color(0xff4658a8),
    2 => const Color(0xff9b5e16),
    _ => const Color(0xff16805d),
  };
}

final class _ZoneSurface extends StatelessWidget {
  const _ZoneSurface({
    required this.zone,
    required this.selected,
    required this.effectiveOpacity,
    required this.opacityFor,
    required this.onSelect,
    required this.selectedZone,
  });

  final DemoZone zone;
  final bool selected;
  final double effectiveOpacity;
  final double Function(DemoZone zone) opacityFor;
  final ValueChanged<DemoZone> onSelect;
  final DemoZone? selectedZone;

  @override
  Widget build(BuildContext context) {
    final content = switch (zone.type) {
      'text' => _text(),
      'image' || 'animated-image' => _image(),
      'chart' => _chart(),
      'video' => _video(),
      'document' => _document(),
      _ => ColoredBox(color: Colors.red.shade100),
    };
    final color = _typeColor(zone.type);
    return Semantics(
      container: true,
      label: '${zone.type}: ${zone.id}',
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: () => onSelect(zone),
        child: DecoratedBox(
          decoration: BoxDecoration(
            border: Border.all(color: color, width: selected ? 4 : 1.5),
          ),
          child: Stack(
            fit: StackFit.expand,
            children: [
              content,
              Align(
                alignment: Alignment.bottomLeft,
                child: IgnorePointer(
                  child: ColoredBox(
                    color: color.withValues(alpha: 0.86),
                    child: Padding(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 4,
                        vertical: 1,
                      ),
                      child: Text(
                        '${zone.type} · ${effectiveOpacity.toStringAsFixed(2)}',
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 8,
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _text() {
    final style = zone.content['style'] as Map<String, Object?>? ?? const {};
    final layout = zone.content['layout'] as Map<String, Object?>? ?? const {};
    final padding = layout['padding'] as Map<String, Object?>? ?? const {};
    final background = style['backgroundColor'] as Map<String, Object?>?;
    final color = style['color'] as Map<String, Object?>?;
    final weight = ((style['fontWeight'] as num?) ?? 400).toDouble();
    final selectable = zone.content['selectable'] as bool? ?? false;
    final textStyle = TextStyle(
      color: _color(color, Colors.black),
      fontSize: ((style['fontSize'] as num?) ?? 16).toDouble(),
      fontWeight: FontWeight.lerp(
        FontWeight.w100,
        FontWeight.w900,
        ((weight - 100) / 800).clamp(0, 1),
      ),
      height: ((style['lineHeightMultiplier'] as num?) ?? 1.2).toDouble(),
      letterSpacing: ((style['letterSpacing'] as num?) ?? 0).toDouble(),
    );
    final text = zone.content['text']! as String;
    final textWidget = selectable
        ? SelectableText(text, style: textStyle)
        : Text(text, style: textStyle, overflow: TextOverflow.clip);
    return Opacity(
      opacity: effectiveOpacity,
      child: ColoredBox(
        color: _color(background, Colors.transparent),
        child: Padding(
          padding: EdgeInsets.fromLTRB(
            ((padding['left'] as num?) ?? 0).toDouble(),
            ((padding['top'] as num?) ?? 0).toDouble(),
            ((padding['right'] as num?) ?? 0).toDouble(),
            ((padding['bottom'] as num?) ?? 0).toDouble(),
          ),
          child: textWidget,
        ),
      ),
    );
  }

  Widget _image() {
    final transform = VisualTransformSpec.fromPlacement(
      zone.content['placement'] as Map<String, Object?>?,
    );
    return Opacity(
      opacity: effectiveOpacity,
      child: transform.applyToVisual(
        _resourceImage(
          zone.resourceAsset!,
          fit: transform.fit,
          alignment: transform.alignment,
          gaplessPlayback: zone.type == 'animated-image',
          filterQuality: FilterQuality.medium,
          errorBuilder: (context, error, stackTrace) => ColoredBox(
            color: const Color(0xffffd5dc),
            child: Center(child: Text('Resource unavailable\n${zone.id}')),
          ),
        ),
        key: ValueKey('visual-transform:${zone.id}'),
      ),
    );
  }

  Widget _resourceImage(
    String resource, {
    required BoxFit fit,
    required Alignment alignment,
    required bool gaplessPlayback,
    required FilterQuality filterQuality,
    required ImageErrorWidgetBuilder errorBuilder,
  }) {
    if (resource.startsWith('file:')) {
      return Image.file(
        File.fromUri(Uri.parse(resource)),
        fit: fit,
        alignment: alignment,
        gaplessPlayback: gaplessPlayback,
        filterQuality: filterQuality,
        errorBuilder: errorBuilder,
      );
    }
    return Image.asset(
      resource,
      fit: fit,
      alignment: alignment,
      gaplessPlayback: gaplessPlayback,
      filterQuality: filterQuality,
      errorBuilder: errorBuilder,
    );
  }

  Widget _chart() {
    final transform = VisualTransformSpec.fromPlacement(
      zone.content['placement'] as Map<String, Object?>?,
    );
    return Opacity(
      opacity: effectiveOpacity,
      child: transform.applyToVisual(
        _InlineBarChart(zone: zone),
        key: ValueKey('visual-transform:${zone.id}'),
      ),
    );
  }

  Widget _video() {
    final transform = VisualTransformSpec.fromPlacement(
      zone.content['placement'] as Map<String, Object?>?,
    );
    final playback =
        zone.content['playback'] as Map<String, Object?>? ?? const {};
    return Opacity(
      opacity: effectiveOpacity,
      child: _EmbeddedVideoSurface(
        key: ValueKey('embedded-video:${zone.id}'),
        resource: zone.resourceAsset!,
        posterResource: zone.posterAsset,
        transform: transform,
        loop: playback['loop'] as bool? ?? false,
      ),
    );
  }

  Widget _document() {
    final child = zone.child!;
    final canvas = SizedBox(
      width: child.width,
      height: child.height,
      child: _DocumentCanvas(
        document: child,
        selected: selectedZone,
        opacityFor: opacityFor,
        onSelect: onSelect,
      ),
    );
    final Widget placed;
    if (zone.documentFit == 'none') {
      placed = ClipRect(
        child: Align(alignment: Alignment.topLeft, child: canvas),
      );
    } else {
      final fit = switch (zone.documentFit) {
        'cover' => BoxFit.cover,
        'fill' => BoxFit.fill,
        _ => BoxFit.contain,
      };
      placed = ClipRect(
        child: FittedBox(fit: fit, child: canvas),
      );
    }
    return Opacity(
      key: ValueKey('document-opacity:${zone.id}'),
      opacity: effectiveOpacity,
      child: placed,
    );
  }

  static Color _typeColor(String type) => switch (type) {
    'text' => const Color(0xff2563eb),
    'image' => const Color(0xffdc2626),
    'animated-image' => const Color(0xff7c3aed),
    'chart' => const Color(0xffd97706),
    'video' => const Color(0xff0f766e),
    _ => const Color(0xff475569),
  };

  static Color _color(Map<String, Object?>? value, Color fallback) {
    if (value == null) return fallback;
    return Color.fromRGBO(
      (((value['red'] as num?) ?? 0) * 255).round(),
      (((value['green'] as num?) ?? 0) * 255).round(),
      (((value['blue'] as num?) ?? 0) * 255).round(),
      ((value['alpha'] as num?) ?? 1).toDouble(),
    );
  }
}

final class _InlineBarChart extends StatelessWidget {
  const _InlineBarChart({required this.zone});

  final DemoZone zone;

  @override
  Widget build(BuildContext context) {
    final rawData = zone.content['data']! as List<Object?>;
    final points = rawData
        .map((value) => value! as Map<String, Object?>)
        .toList(growable: false);
    final maximum = math.max(
      1.0,
      points
          .map((point) => (point['value']! as num).toDouble())
          .reduce(math.max),
    );
    final title = zone.content['title'] as String? ?? 'Chart';
    return ColoredBox(
      color: Theme.of(context).colorScheme.surface.withValues(alpha: 0.94),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(10, 8, 10, 8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              title,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.titleSmall,
            ),
            const SizedBox(height: 6),
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  for (final point in points)
                    Expanded(
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 3),
                        child: Column(
                          children: [
                            Expanded(
                              child: Align(
                                alignment: Alignment.bottomCenter,
                                child: FractionallySizedBox(
                                  key: ValueKey(
                                    'chart-bar:${zone.id}:${point['label']}',
                                  ),
                                  widthFactor: 0.62,
                                  heightFactor:
                                      ((point['value']! as num).toDouble() /
                                              maximum)
                                          .clamp(0.02, 1),
                                  child: DecoratedBox(
                                    decoration: BoxDecoration(
                                      color: _chartColor(
                                        point['color'] as String?,
                                      ),
                                      borderRadius: const BorderRadius.vertical(
                                        top: Radius.circular(4),
                                      ),
                                    ),
                                  ),
                                ),
                              ),
                            ),
                            const SizedBox(height: 3),
                            Text(
                              '${point['label']} ${point['value']}',
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                              textAlign: TextAlign.center,
                              style: const TextStyle(fontSize: 9),
                            ),
                          ],
                        ),
                      ),
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  static Color _chartColor(String? value) {
    if (value == null || !RegExp(r'^#[0-9a-fA-F]{6}$').hasMatch(value)) {
      return const Color(0xff2563eb);
    }
    return Color(int.parse(value.substring(1), radix: 16) | 0xff000000);
  }
}

final class _EmbeddedVideoSurface extends StatefulWidget {
  const _EmbeddedVideoSurface({
    required this.resource,
    required this.posterResource,
    required this.transform,
    required this.loop,
    super.key,
  });

  final String resource;
  final String? posterResource;
  final VisualTransformSpec transform;
  final bool loop;

  @override
  State<_EmbeddedVideoSurface> createState() => _EmbeddedVideoSurfaceState();
}

final class _EmbeddedVideoSurfaceState extends State<_EmbeddedVideoSurface> {
  final List<StreamSubscription<dynamic>> _subscriptions = [];
  Player? _player;
  VideoController? _controller;
  Future<void>? _initialization;
  String? _error;
  var _ready = false;
  var _playing = false;
  var _disposed = false;

  @override
  void dispose() {
    _disposed = true;
    unawaited(_close());
    super.dispose();
  }

  Future<void> _close() async {
    await _initialization;
    for (final subscription in _subscriptions) {
      await subscription.cancel();
    }
    _subscriptions.clear();
    await _player?.dispose();
    _player = null;
    _controller = null;
  }

  Future<void> _toggle() async {
    await (_initialization ??= _initialize());
    final player = _player;
    if (_disposed || !_ready || player == null) return;
    if (_playing) {
      await player.pause();
    } else {
      await player.play();
    }
  }

  Future<void> _initialize() async {
    final player = Player();
    final controller = VideoController(player);
    _player = player;
    _controller = controller;
    _subscriptions.addAll([
      player.stream.playing.listen((value) {
        if (_disposed || !mounted) return;
        setState(() => _playing = value);
      }),
      player.stream.error.listen((value) {
        if (_disposed || !mounted) return;
        setState(() => _error = value);
      }),
    ]);
    if (mounted) setState(() {});
    try {
      final path = await _resolveMediaPath();
      if (_disposed) return;
      if (widget.loop) await player.setPlaylistMode(PlaylistMode.loop);
      await player.open(Media(path), play: false);
      if (_disposed || !mounted) return;
      setState(() => _ready = true);
    } on Object catch (error) {
      if (_disposed || !mounted) return;
      setState(() => _error = error.toString());
    }
  }

  Future<String> _resolveMediaPath() async {
    if (widget.resource.startsWith('file:')) {
      return File.fromUri(Uri.parse(widget.resource)).path;
    }
    final data = await rootBundle.load(widget.resource);
    final temporary = await getTemporaryDirectory();
    final directory = Directory(
      '${temporary.path}${Platform.pathSeparator}'
      'facetwire-playground-rich-media-v0.1',
    );
    await directory.create(recursive: true);
    final assetName = Uri.parse(widget.resource).pathSegments.last;
    final safeName = '${widget.resource.hashCode}-$assetName';
    final file = File('${directory.path}${Platform.pathSeparator}$safeName');
    if (!await file.exists() || await file.length() != data.lengthInBytes) {
      await file.writeAsBytes(
        data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
        flush: true,
      );
    }
    return file.path;
  }

  @override
  Widget build(BuildContext context) {
    final controller = _controller;
    return ColoredBox(
      color: Colors.black,
      child: Stack(
        fit: StackFit.expand,
        children: [
          if (_ready && controller != null)
            widget.transform.applyToVisual(
              Video(
                key: const ValueKey('embedded-video-surface'),
                controller: controller,
                controls: NoVideoControls,
                fit: widget.transform.fit,
                fill: Colors.transparent,
              ),
              key: const ValueKey('embedded-video-visual-transform'),
            )
          else
            widget.transform.applyToVisual(
              _poster(),
              key: const ValueKey('embedded-video-poster-transform'),
            ),
          if (_initialization != null && !_ready && _error == null)
            const Center(child: CircularProgressIndicator()),
          Center(
            child: IconButton.filledTonal(
              key: const ValueKey('embedded-video-play-pause'),
              tooltip: _playing ? '暂停视频' : '播放视频',
              onPressed: _error == null ? _toggle : null,
              iconSize: 30,
              icon: Icon(_playing ? Icons.pause : Icons.play_arrow),
            ),
          ),
          if (_error != null)
            Align(
              alignment: Alignment.bottomCenter,
              child: ColoredBox(
                color: Theme.of(context).colorScheme.errorContainer,
                child: Padding(
                  padding: const EdgeInsets.all(6),
                  child: Text(
                    'Video unavailable: $_error',
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _poster() {
    final poster = widget.posterResource;
    if (poster == null) {
      return const Center(
        child: Icon(Icons.movie_outlined, color: Colors.white70, size: 64),
      );
    }
    Widget errorBuilder(
      BuildContext context,
      Object error,
      StackTrace? stackTrace,
    ) => const Center(
      child: Icon(Icons.broken_image_outlined, color: Colors.white70, size: 48),
    );
    if (poster.startsWith('file:')) {
      return Image.file(
        File.fromUri(Uri.parse(poster)),
        fit: widget.transform.fit,
        alignment: widget.transform.alignment,
        errorBuilder: errorBuilder,
      );
    }
    return Image.asset(
      poster,
      fit: widget.transform.fit,
      alignment: widget.transform.alignment,
      errorBuilder: errorBuilder,
    );
  }
}
