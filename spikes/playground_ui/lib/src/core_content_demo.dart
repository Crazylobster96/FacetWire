// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

const coreContentDemoDescriptor =
    'assets/documents/core-content-overlap-demo.agscene/'
    'core-content-overlap-demo.agscene.dis.json';

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
    required this.child,
    required this.documentFit,
  });

  final String key;
  final String id;
  final DemoBounds bounds;
  final Map<String, Object?> content;
  final String? resourceAsset;
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
          } else if (type == 'image' || type == 'animated-image') {
            final resource = _string(
              content['resource'],
              '$zoneId.content.resource',
            );
            resourceAsset = resources[resource];
            if (resourceAsset == null) {
              throw FormatException('$zoneId references unknown $resource');
            }
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
          } else if (type == 'image' || type == 'animated-image') {
            final resource = CoreContentPackageLoader._string(
              content['resource'],
              '$zoneId.content.resource',
            );
            resourceAsset = resources[resource];
            if (resourceAsset == null) {
              throw FormatException('$zoneId references unknown $resource');
            }
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
    this.descriptorAsset = coreContentDemoDescriptor,
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
        title: const Text('Text + Core Image 0.1 Demo'),
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
            child: Text('三层原始坐标递归 · Text / Image / GIF'),
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
          for (final type in const ['text', 'image', 'animated-image'])
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
      _ => 'GIF',
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
      decoration: BoxDecoration(
        color: const Color(0xfff8f7fc),
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
    final placement =
        zone.content['placement'] as Map<String, Object?>? ?? const {};
    final alignmentValue =
        placement['alignment'] as Map<String, Object?>? ?? const {};
    final fit = switch (placement['fit'] as String? ?? 'contain') {
      'none' => BoxFit.none,
      'cover' => BoxFit.cover,
      'fill' => BoxFit.fill,
      _ => BoxFit.contain,
    };
    final alignment = Alignment(
      (((alignmentValue['x'] as num?) ?? 0.5).toDouble() * 2) - 1,
      (((alignmentValue['y'] as num?) ?? 0.5).toDouble() * 2) - 1,
    );
    return Opacity(
      opacity: effectiveOpacity,
      child: _resourceImage(
        zone.resourceAsset!,
        fit: fit,
        alignment: alignment,
        gaplessPlayback: zone.type == 'animated-image',
        filterQuality: FilterQuality.medium,
        errorBuilder: (context, error, stackTrace) => ColoredBox(
          color: const Color(0xffffd5dc),
          child: Center(child: Text('Resource unavailable\n${zone.id}')),
        ),
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
    if (zone.documentFit == 'none') {
      return ClipRect(
        child: Align(alignment: Alignment.topLeft, child: canvas),
      );
    }
    final fit = switch (zone.documentFit) {
      'cover' => BoxFit.cover,
      'fill' => BoxFit.fill,
      _ => BoxFit.contain,
    };
    return ClipRect(
      child: FittedBox(fit: fit, child: canvas),
    );
  }

  static Color _typeColor(String type) => switch (type) {
    'text' => const Color(0xff2563eb),
    'image' => const Color(0xffdc2626),
    'animated-image' => const Color(0xff7c3aed),
    _ => const Color(0xff0f766e),
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
