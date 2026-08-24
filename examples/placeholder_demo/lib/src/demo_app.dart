// SPDX-License-Identifier: MPL-2.0
import 'dart:async';

import 'package:flutter/material.dart';

import 'demo_models.dart';
import 'package_loader.dart';

const String demoDescriptorAsset =
    'assets/documents/recursive-placeholder-demo.agscene/'
    'recursive-placeholder-demo.agscene.dis.json';

class PlaceholderDemoApp extends StatelessWidget {
  const PlaceholderDemoApp({
    required this.client,
    required this.packageLoader,
    super.key,
  });

  final NativeDemoClient client;
  final AgscenePackageLoader packageLoader;

  @override
  Widget build(BuildContext context) => MaterialApp(
    debugShowCheckedModeBanner: false,
    title: 'FacetWire Placeholder Demo',
    theme: ThemeData(
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xff4169e1),
        brightness: Brightness.light,
      ),
      useMaterial3: true,
    ),
    darkTheme: ThemeData(
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xff7da2ff),
        brightness: Brightness.dark,
      ),
      useMaterial3: true,
    ),
    home: PlaceholderDemoScreen(client: client, packageLoader: packageLoader),
  );
}

class PlaceholderDemoScreen extends StatefulWidget {
  const PlaceholderDemoScreen({
    required this.client,
    required this.packageLoader,
    super.key,
  });

  final NativeDemoClient client;
  final AgscenePackageLoader packageLoader;

  @override
  State<PlaceholderDemoScreen> createState() => _PlaceholderDemoScreenState();
}

class _PlaceholderDemoScreenState extends State<PlaceholderDemoScreen> {
  SceneDocument? _document;
  SceneZone? _selectedZone;
  Map<String, Object?>? _snapshot;
  Map<String, Object?>? _schema;
  final Map<String, DemoRenderReport> _reports = {};
  Object? _error;
  var _busy = true;
  var _refreshToken = 0;
  var _revision = 1;

  double _width = 650;
  double _height = 366;
  double _opacity = .82;
  double _backgroundAlpha = .72;
  double _fontScale = 1;
  PlaceholderReason _reason = PlaceholderReason.resourceUnavailable;
  PlaceholderMode _mode = PlaceholderMode.standard;
  PlaceholderPhase _phase = PlaceholderPhase.transferring;
  MeasureCase _measureCase = MeasureCase.resolved;
  int _permittedActions = PlaceholderAction.all;
  double _progress = .45;
  bool _stale = false;
  bool _prefersDark = false;
  bool _highContrast = false;
  bool _reduceMotion = false;

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  Future<void> _initialize() async {
    try {
      final results = await Future.wait<Object>([
        widget.packageLoader.load(demoDescriptorAsset),
        widget.client.snapshot(),
        widget.client.parameterSchema(),
      ]);
      final document = results[0] as SceneDocument;
      final selected = document.placeholderZones.first;
      if (!mounted) return;
      setState(() {
        _document = document;
        _snapshot = results[1] as Map<String, Object?>;
        _schema = results[2] as Map<String, Object?>;
        _selectedZone = selected;
        _width = selected.bounds.width.clamp(160, 960);
        _height = selected.bounds.height.clamp(90, 640);
        _reason = selected.reason;
        _mode = selected.mode;
        _permittedActions = selected.permittedActions;
        _phase = _phaseForReason(selected.reason);
        _busy = false;
      });
      await _refreshAll(document);
    } on Object catch (error) {
      if (mounted) {
        setState(() {
          _error = error;
          _busy = false;
        });
      }
    }
  }

  void _selectZone(SceneZone zone) {
    setState(() {
      _selectedZone = zone;
      _width = zone.bounds.width.clamp(160, 960);
      _height = zone.bounds.height.clamp(90, 640);
      _reason = zone.reason;
      _mode = zone.mode;
      _permittedActions = zone.permittedActions;
      _phase = _phaseForReason(zone.reason);
      _revision += 1;
    });
    unawaited(_refresh());
  }

  static PlaceholderPhase _phaseForReason(PlaceholderReason reason) =>
      switch (reason) {
        PlaceholderReason.loading => PlaceholderPhase.running,
        PlaceholderReason.resourceUnavailable => PlaceholderPhase.transferring,
        PlaceholderReason.rendererMissing => PlaceholderPhase.readyForHandoff,
        _ => PlaceholderPhase.waiting,
      };

  DemoNativeRequest get _request =>
      _requestForZone(_selectedZone!, useSelectedInputs: true);

  DemoNativeRequest _requestForZone(
    SceneZone zone, {
    required bool useSelectedInputs,
  }) {
    return DemoNativeRequest(
      width: useSelectedInputs ? _width : zone.bounds.width.clamp(160, 960),
      height: useSelectedInputs ? _height : zone.bounds.height.clamp(90, 640),
      opacity: useSelectedInputs ? _opacity : .82,
      backgroundAlpha: useSelectedInputs ? _backgroundAlpha : .72,
      fontScale: useSelectedInputs ? _fontScale : 1,
      devicePixelRatio: MediaQuery.devicePixelRatioOf(context),
      reason: useSelectedInputs ? _reason : zone.reason,
      mode: useSelectedInputs ? _mode : zone.mode,
      permittedActions: useSelectedInputs
          ? _permittedActions
          : zone.permittedActions,
      phase: useSelectedInputs ? _phase : _phaseForReason(zone.reason),
      progressKind:
          (useSelectedInputs ? _phase : _phaseForReason(zone.reason)) ==
              PlaceholderPhase.none
          ? 0
          : 2,
      completed: ((useSelectedInputs ? _progress : .45) * 100).round(),
      total: 100,
      stale: useSelectedInputs && _stale,
      prefersDark: useSelectedInputs && _prefersDark,
      highContrast: useSelectedInputs && _highContrast,
      reduceMotion: useSelectedInputs && _reduceMotion,
      measureCase: useSelectedInputs ? _measureCase : MeasureCase.resolved,
      presentationRevision: _revision,
      contentKind: zone.kind,
      label: zone.label,
    );
  }

  Future<void> _refreshAll(SceneDocument document) async {
    final token = ++_refreshToken;
    final zones = document.placeholderZones.toList(growable: false);
    try {
      final reports = await Future.wait([
        for (final zone in zones)
          widget.client.render(
            _requestForZone(
              zone,
              useSelectedInputs: identical(zone, _selectedZone),
            ),
          ),
      ]);
      if (mounted && token == _refreshToken) {
        setState(() {
          _reports
            ..clear()
            ..addEntries(
              List.generate(
                zones.length,
                (index) => MapEntry(zones[index].id, reports[index]),
              ),
            );
          _error = null;
        });
      }
    } on Object catch (error) {
      if (mounted && token == _refreshToken) {
        setState(() => _error = error);
      }
    }
  }

  Future<void> _refresh() async {
    final token = ++_refreshToken;
    try {
      final report = await widget.client.render(_request);
      if (mounted && token == _refreshToken) {
        setState(() {
          _reports[_selectedZone!.id] = report;
          _error = null;
        });
      }
    } on Object catch (error) {
      if (mounted && token == _refreshToken) {
        setState(() => _error = error);
      }
    }
  }

  void _change(VoidCallback mutation, {bool refresh = true}) {
    setState(() {
      mutation();
      _revision += 1;
    });
    if (refresh) unawaited(_refresh());
  }

  @override
  void dispose() {
    unawaited(widget.client.close());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final snapshot = _snapshot;
    return Scaffold(
      appBar: AppBar(
        title: const Text('FacetWire Placeholder Renderer Demo'),
        actions: [
          if (snapshot != null)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8),
              child: Chip(
                avatar: const Icon(Icons.check_circle, size: 18),
                label: Text('${snapshot['pluginVersion']} · real C ABI'),
              ),
            ),
          IconButton(
            tooltip: 'Parameter Schema',
            onPressed: _schema == null ? null : _showSchema,
            icon: const Icon(Icons.data_object),
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: _busy
          ? const Center(child: CircularProgressIndicator())
          : _document == null
          ? _ErrorView(error: _error)
          : Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                SizedBox(width: 286, child: _buildSceneTree(_document!)),
                const VerticalDivider(width: 1),
                Expanded(child: _buildPreview()),
                const VerticalDivider(width: 1),
                SizedBox(width: 350, child: _buildInspector()),
              ],
            ),
    );
  }

  Widget _buildSceneTree(SceneDocument document) => Material(
    color: Theme.of(context).colorScheme.surfaceContainerLow,
    child: ListView(
      padding: const EdgeInsets.symmetric(vertical: 8),
      children: [
        const ListTile(
          leading: Icon(Icons.account_tree_outlined),
          title: Text('ASP scene tree'),
          subtitle: Text('3 recursive document levels'),
        ),
        const Divider(),
        _documentTile(document),
      ],
    ),
  );

  Widget _documentTile(SceneDocument document) => ExpansionTile(
    key: PageStorageKey(document.id),
    initiallyExpanded: true,
    leading: CircleAvatar(child: Text('${document.depth}')),
    title: Text(document.title, maxLines: 2),
    subtitle: Text(
      '${document.canvasId} · '
      '${document.canvasWidth.toInt()} × ${document.canvasHeight.toInt()}',
    ),
    children: [
      for (final page in document.pages)
        ...page.layers.expand(
          (layer) => layer.zones.map((zone) {
            final child = zone.child;
            if (child != null) {
              final placement = zone.documentPlacement!;
              return ExpansionTile(
                key: PageStorageKey(zone.id),
                initiallyExpanded: true,
                leading: const Icon(Icons.select_all_outlined),
                title: Text(zone.id, maxLines: 1),
                subtitle: Text(
                  'document Zone · ${zone.bounds.width.toInt()} × '
                  '${zone.bounds.height.toInt()} · ${placement.fit.name}',
                ),
                children: [
                  Padding(
                    padding: const EdgeInsets.only(left: 14),
                    child: _documentTile(child),
                  ),
                ],
              );
            }
            return ListTile(
              key: ValueKey('tree:${zone.id}'),
              selected: identical(zone, _selectedZone),
              leading: Icon(_kindIcon(zone.kind)),
              title: Text(zone.id, maxLines: 1),
              subtitle: Text('${zone.kind} · ${zone.reason.documentValue}'),
              onTap: () => _selectZone(zone),
            );
          }),
        ),
    ],
  );

  IconData _kindIcon(String kind) => switch (kind) {
    'image' => Icons.image_outlined,
    'video' => Icons.movie_outlined,
    'chart' => Icons.insert_chart_outlined,
    _ => Icons.crop_square,
  };

  Widget _buildPreview() {
    final document = _document!;
    final selected = _selectedZone;
    final report = selected == null ? null : _reports[selected.id];
    final error = _error;
    return ColoredBox(
      color: Theme.of(context).colorScheme.surfaceContainerLowest,
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Composed recursive scene / 递归组合场景',
                        style: Theme.of(context).textTheme.titleMedium,
                      ),
                      Text(
                        _selectedPath(),
                        key: const ValueKey('selected-zone-path'),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
                ),
                Text(
                  '${document.canvasWidth.round()} × '
                  '${document.canvasHeight.round()} root canvas',
                ),
              ],
            ),
            const SizedBox(height: 12),
            Expanded(
              child: Center(
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 920),
                  child: AspectRatio(
                    aspectRatio: document.canvasWidth / document.canvasHeight,
                    child: error != null
                        ? _ErrorView(error: error)
                        : _reports.length < document.placeholderZones.length
                        ? const Center(child: CircularProgressIndicator())
                        : _SceneCompositionPreview(
                            document: document,
                            reports: _reports,
                            selectedZone: selected,
                            onZoneTap: _handleZoneTap,
                          ),
                  ),
                ),
              ),
            ),
            const SizedBox(height: 16),
            if (report != null) _ReportStrip(report: report),
          ],
        ),
      ),
    );
  }

  String _selectedPath() {
    final selected = _selectedZone;
    final document = _document;
    if (selected == null || document == null) return 'No Zone selected';
    return _findZonePath(document, selected)?.join(' → ') ?? selected.id;
  }

  List<String>? _findZonePath(SceneDocument document, SceneZone target) {
    for (final zone in document.zones) {
      if (identical(zone, target)) {
        return [document.canvasId, zone.id];
      }
      final child = zone.child;
      if (child != null) {
        final nested = _findZonePath(child, target);
        if (nested != null) return [document.canvasId, zone.id, ...nested];
      }
    }
    return null;
  }

  Future<void> _handleZoneTap(SceneZone zone, double x, double y) async {
    if (!identical(zone, _selectedZone)) _selectZone(zone);
    await _handleCanvasTap(x, y);
  }

  Future<void> _handleCanvasTap(double x, double y) async {
    final action = await widget.client.hitTest(_request, x, y);
    if (!mounted) return;
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(
        SnackBar(
          content: Text(
            action == 0
                ? 'Hit test: no action at (${x.round()}, ${y.round()})'
                : 'Hit test: ${PlaceholderAction.label(action)}',
          ),
        ),
      );
  }

  Widget _buildInspector() => Material(
    color: Theme.of(context).colorScheme.surfaceContainerLow,
    child: ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text('Renderer inputs', style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 16),
        _dropdown<PlaceholderReason>(
          label: 'Reason',
          value: _reason,
          values: PlaceholderReason.values,
          text: (value) => value.documentValue,
          onChanged: (value) => _change(() => _reason = value),
        ),
        const SizedBox(height: 12),
        _dropdown<PlaceholderMode>(
          label: 'Mode',
          value: _mode,
          values: PlaceholderMode.values,
          text: (value) => value.documentValue,
          onChanged: (value) => _change(() => _mode = value),
        ),
        const SizedBox(height: 12),
        _dropdown<PlaceholderPhase>(
          label: 'Presentation phase',
          value: _phase,
          values: PlaceholderPhase.values,
          text: (value) => value.label,
          onChanged: (value) => _change(() => _phase = value),
        ),
        const SizedBox(height: 12),
        _dropdown<MeasureCase>(
          label: 'Measure source scenario',
          value: _measureCase,
          values: MeasureCase.values,
          text: (value) => value.label,
          onChanged: (value) => _change(() => _measureCase = value),
        ),
        const SizedBox(height: 20),
        _slider(
          key: const ValueKey('opacity-slider'),
          label: '不透明度 / Opacity',
          value: _opacity,
          valueLabel: '${(_opacity * 100).round()}%',
          onChanged: (value) => setState(() => _opacity = value),
          onChangeEnd: (value) => _change(() => _opacity = value),
        ),
        _slider(
          label: 'Background alpha',
          value: _backgroundAlpha,
          valueLabel: '${(_backgroundAlpha * 100).round()}%',
          onChanged: (value) => setState(() => _backgroundAlpha = value),
          onChangeEnd: (value) => _change(() => _backgroundAlpha = value),
        ),
        Text(
          'Final background alpha = color alpha × opacity = '
          '${(_backgroundAlpha * _opacity * 100).round()}%',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 12),
        _slider(
          label: 'Font scale',
          value: _fontScale,
          min: .75,
          max: 2,
          valueLabel: '${_fontScale.toStringAsFixed(2)}×',
          onChanged: (value) => setState(() => _fontScale = value),
          onChangeEnd: (value) => _change(() => _fontScale = value),
        ),
        _slider(
          label: 'Progress',
          value: _progress,
          valueLabel: '${(_progress * 100).round()}%',
          onChanged: (value) => setState(() => _progress = value),
          onChangeEnd: (value) => _change(() => _progress = value),
        ),
        const Divider(height: 28),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Stale presentation'),
          value: _stale,
          onChanged: (value) => _change(() => _stale = value),
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Dark target'),
          value: _prefersDark,
          onChanged: (value) => _change(() => _prefersDark = value),
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('High contrast'),
          value: _highContrast,
          onChanged: (value) => _change(() => _highContrast = value),
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Reduce motion'),
          value: _reduceMotion,
          onChanged: (value) => _change(() => _reduceMotion = value),
        ),
      ],
    ),
  );

  Widget _slider({
    Key? key,
    required String label,
    required double value,
    required String valueLabel,
    required ValueChanged<double> onChanged,
    required ValueChanged<double> onChangeEnd,
    double min = 0,
    double max = 1,
  }) => Column(
    key: key,
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [Text(label), Text(valueLabel)],
      ),
      Slider(
        value: value,
        min: min,
        max: max,
        onChanged: onChanged,
        onChangeEnd: onChangeEnd,
      ),
    ],
  );

  Widget _dropdown<T>({
    required String label,
    required T value,
    required List<T> values,
    required String Function(T) text,
    required ValueChanged<T> onChanged,
  }) => DropdownButtonFormField<T>(
    isExpanded: true,
    initialValue: value,
    key: ValueKey('$label:$value'),
    decoration: InputDecoration(
      labelText: label,
      border: const OutlineInputBorder(),
    ),
    items: [
      for (final item in values)
        DropdownMenuItem(value: item, child: Text(text(item))),
    ],
    onChanged: (selected) {
      if (selected != null) onChanged(selected);
    },
  );

  void _showSchema() {
    showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Renderer parameter schema'),
        content: SizedBox(
          width: 640,
          child: SelectableText(_prettyMap(_schema!)),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Close'),
          ),
        ],
      ),
    );
  }

  static String _prettyMap(Map<String, Object?> value) =>
      value.entries.map((entry) => '${entry.key}: ${entry.value}').join('\n');
}

final class _SceneCompositionPreview extends StatefulWidget {
  const _SceneCompositionPreview({
    required this.document,
    required this.reports,
    required this.selectedZone,
    required this.onZoneTap,
  });

  final SceneDocument document;
  final Map<String, DemoRenderReport> reports;
  final SceneZone? selectedZone;
  final void Function(SceneZone zone, double x, double y) onZoneTap;

  @override
  State<_SceneCompositionPreview> createState() =>
      _SceneCompositionPreviewState();
}

final class _SceneCompositionPreviewState
    extends State<_SceneCompositionPreview> {
  final _horizontalController = ScrollController();
  final _verticalController = ScrollController();

  @override
  void dispose() {
    _horizontalController.dispose();
    _verticalController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => ColoredBox(
    key: const ValueKey('composed-scene-preview'),
    color: Theme.of(context).colorScheme.surfaceContainerHighest,
    child: Scrollbar(
      controller: _horizontalController,
      thumbVisibility: true,
      scrollbarOrientation: ScrollbarOrientation.bottom,
      notificationPredicate: (notification) =>
          notification.metrics.axis == Axis.horizontal,
      child: SingleChildScrollView(
        key: const ValueKey('scene-100-percent-viewport'),
        controller: _horizontalController,
        scrollDirection: Axis.horizontal,
        child: Scrollbar(
          controller: _verticalController,
          thumbVisibility: true,
          scrollbarOrientation: ScrollbarOrientation.right,
          notificationPredicate: (notification) =>
              notification.metrics.axis == Axis.vertical,
          child: SingleChildScrollView(
            controller: _verticalController,
            scrollDirection: Axis.vertical,
            child: SizedBox(
              key: const ValueKey('root-canvas-surface'),
              width: widget.document.canvasWidth,
              height: widget.document.canvasHeight,
              child: _DocumentSurface(
                document: widget.document,
                reports: widget.reports,
                selectedZone: widget.selectedZone,
                onZoneTap: widget.onZoneTap,
                root: true,
              ),
            ),
          ),
        ),
      ),
    ),
  );
}

final class _DocumentSurface extends StatelessWidget {
  const _DocumentSurface({
    required this.document,
    required this.reports,
    required this.selectedZone,
    required this.onZoneTap,
    this.root = false,
  });

  final SceneDocument document;
  final Map<String, DemoRenderReport> reports;
  final SceneZone? selectedZone;
  final void Function(SceneZone zone, double x, double y) onZoneTap;
  final bool root;

  @override
  Widget build(BuildContext context) {
    final page = document.pages.first;
    final surface = Stack(
      clipBehavior: Clip.hardEdge,
      children: [
        Positioned.fill(
          child: ColoredBox(
            color: root
                ? Theme.of(context).colorScheme.surface
                : Theme.of(context).colorScheme.surfaceContainerLowest,
          ),
        ),
        for (final layer in page.layers)
          for (final zone in layer.zones) _zone(context, zone, layer),
        Positioned(
          left: 8,
          top: 6,
          child: _SceneBadge(
            '${document.depth} · ${document.canvasId}',
            color: Theme.of(context).colorScheme.primary,
          ),
        ),
      ],
    );
    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(
          color: Theme.of(context).colorScheme.primary.withValues(alpha: .55),
          width: root ? 3 : 2,
        ),
      ),
      child: surface,
    );
  }

  Widget _zone(BuildContext context, SceneZone zone, SceneLayer layer) {
    final bounds = zone.bounds;
    return Positioned(
      key: ValueKey('scene-zone:${zone.id}'),
      left: bounds.x,
      top: bounds.y,
      width: bounds.width,
      height: bounds.height,
      child: zone.child == null
          ? _PlaceholderZoneSurface(
              zone: zone,
              report: reports[zone.id],
              selected: identical(zone, selectedZone),
              layerId: layer.id,
              onTap: onZoneTap,
            )
          : _NestedDocumentSurface(
              zone: zone,
              reports: reports,
              selectedZone: selectedZone,
              layerId: layer.id,
              onZoneTap: onZoneTap,
            ),
    );
  }
}

final class _NestedDocumentSurface extends StatelessWidget {
  const _NestedDocumentSurface({
    required this.zone,
    required this.reports,
    required this.selectedZone,
    required this.layerId,
    required this.onZoneTap,
  });

  final SceneZone zone;
  final Map<String, DemoRenderReport> reports;
  final SceneZone? selectedZone;
  final String layerId;
  final void Function(SceneZone zone, double x, double y) onZoneTap;

  @override
  Widget build(BuildContext context) {
    final child = zone.child!;
    final placement = zone.documentPlacement!;
    final fitted = FittedBox(
      fit: switch (placement.fit) {
        SceneDocumentFit.none => BoxFit.none,
        SceneDocumentFit.contain => BoxFit.contain,
        SceneDocumentFit.cover => BoxFit.cover,
        SceneDocumentFit.fill => BoxFit.fill,
      },
      alignment: Alignment(
        placement.alignmentX * 2 - 1,
        placement.alignmentY * 2 - 1,
      ),
      child: SizedBox(
        width: child.canvasWidth,
        height: child.canvasHeight,
        child: _DocumentSurface(
          document: child,
          reports: reports,
          selectedZone: selectedZone,
          onZoneTap: onZoneTap,
        ),
      ),
    );
    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(
          color: Theme.of(context).colorScheme.tertiary,
          width: 3,
        ),
      ),
      child: Stack(
        fit: StackFit.expand,
        children: [
          ClipRect(
            clipBehavior: placement.clip ? Clip.hardEdge : Clip.none,
            child: fitted,
          ),
          Positioned(
            right: 6,
            top: 6,
            child: _SceneBadge(
              '${zone.id} · $layerId · '
              '@ ${zone.bounds.x.round()},${zone.bounds.y.round()} · '
              '${zone.bounds.width.round()}×${zone.bounds.height.round()} · ${placement.fit.name}',
              color: Theme.of(context).colorScheme.tertiary,
            ),
          ),
        ],
      ),
    );
  }
}

final class _PlaceholderZoneSurface extends StatelessWidget {
  const _PlaceholderZoneSurface({
    required this.zone,
    required this.report,
    required this.selected,
    required this.layerId,
    required this.onTap,
  });

  final SceneZone zone;
  final DemoRenderReport? report;
  final bool selected;
  final String layerId;
  final void Function(SceneZone zone, double x, double y) onTap;

  @override
  Widget build(BuildContext context) => Semantics(
    container: true,
    image: true,
    label: report?.semantics['label'] as String? ?? zone.label,
    value: report?.semantics['statusKey'] as String? ?? 'loading',
    child: GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTapUp: (details) =>
          onTap(zone, details.localPosition.dx, details.localPosition.dy),
      child: Stack(
        fit: StackFit.expand,
        children: [
          if (report == null)
            const Center(child: CircularProgressIndicator())
          else
            CustomPaint(
              key: ValueKey('placeholder-display-list:${zone.id}'),
              foregroundPainter: _DisplayListPainter(
                report!.commands,
                Size(zone.bounds.width, zone.bounds.height),
              ),
            ),
          DecoratedBox(
            decoration: BoxDecoration(
              border: Border.all(
                color: selected
                    ? Theme.of(context).colorScheme.error
                    : Theme.of(context).colorScheme.outline,
                width: selected ? 5 : 2,
              ),
            ),
          ),
          Positioned(
            left: 6,
            bottom: 6,
            child: _SceneBadge(
              '${zone.id} · $layerId · '
              '@ ${zone.bounds.x.round()},${zone.bounds.y.round()} · '
              '${zone.bounds.width.round()}×${zone.bounds.height.round()}',
              color: selected
                  ? Theme.of(context).colorScheme.error
                  : Theme.of(context).colorScheme.outline,
            ),
          ),
        ],
      ),
    ),
  );
}

final class _SceneBadge extends StatelessWidget {
  const _SceneBadge(this.label, {required this.color});
  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
    color: color.withValues(alpha: .88),
    child: Text(
      label,
      style: const TextStyle(color: Colors.white, fontSize: 12),
    ),
  );
}

final class _DisplayListPainter extends CustomPainter {
  const _DisplayListPainter(this.commands, this.logicalSize);
  final List<DemoCommand> commands;
  final Size logicalSize;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.scale(
      size.width / logicalSize.width,
      size.height / logicalSize.height,
    );
    for (final command in commands) {
      final rect = Rect.fromLTWH(
        command.x,
        command.y,
        command.width,
        command.height,
      );
      final color = Color.fromRGBO(
        (command.red * 255).round().clamp(0, 255),
        (command.green * 255).round().clamp(0, 255),
        (command.blue * 255).round().clamp(0, 255),
        command.alpha.clamp(0, 1),
      );
      switch (command.op) {
        case 'save':
          canvas.save();
        case 'restore':
          canvas.restore();
        case 'clipRect':
          canvas.clipRect(rect);
        case 'fillRoundedRect':
          canvas.drawRRect(
            RRect.fromRectAndRadius(rect, Radius.circular(command.radius)),
            Paint()..color = color,
          );
        case 'strokeRoundedRect':
          canvas.drawRRect(
            RRect.fromRectAndRadius(rect, Radius.circular(command.radius)),
            Paint()
              ..color = color
              ..style = PaintingStyle.stroke
              ..strokeWidth = command.strokeWidth,
          );
        case 'symbol':
          _drawSymbol(canvas, rect, color, command.value);
        case 'text':
          final text = TextPainter(
            text: TextSpan(
              text: command.value,
              style: TextStyle(color: color, fontSize: command.height),
            ),
            maxLines: 2,
            ellipsis: '…',
            textDirection: TextDirection.ltr,
          )..layout(maxWidth: logicalSize.width - command.x - 16);
          text.paint(canvas, Offset(command.x + 44, command.y));
      }
    }
  }

  static void _drawSymbol(Canvas canvas, Rect rect, Color color, String value) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.5;
    canvas.drawRRect(
      RRect.fromRectAndRadius(rect, Radius.circular(rect.shortestSide * .22)),
      paint,
    );
    if (value.contains('loading')) {
      canvas.drawArc(rect.deflate(7), -.8, 4.8, false, paint);
    } else {
      canvas.drawLine(
        rect.topLeft + const Offset(8, 8),
        rect.bottomRight - const Offset(8, 8),
        paint,
      );
      canvas.drawLine(
        rect.topRight + const Offset(-8, 8),
        rect.bottomLeft + const Offset(8, -8),
        paint,
      );
    }
  }

  @override
  bool shouldRepaint(_DisplayListPainter oldDelegate) =>
      !identical(oldDelegate.commands, commands) ||
      oldDelegate.logicalSize != logicalSize;
}

final class _ReportStrip extends StatelessWidget {
  const _ReportStrip({required this.report});
  final DemoRenderReport report;

  @override
  Widget build(BuildContext context) => Wrap(
    spacing: 8,
    runSpacing: 8,
    children: [
      _Metric('Validate', '${report.contract['validationStatus']}'),
      _Metric(
        'Measure',
        '${report.measure['width']} × ${report.measure['height']} '
            '(source ${report.measure['source']})',
      ),
      _Metric('Density', '${report.render['visualDensity']}'),
      _Metric('Commands', '${report.render['commandCount']}'),
      _Metric('Actions', '${report.render['visibleActions']}'),
      _Metric('Semantics role', '${report.semantics['role']}'),
      _Metric('Phase', '${report.semantics['phase']}'),
      _Metric('Stale', '${report.semantics['stale']}'),
    ],
  );
}

final class _Metric extends StatelessWidget {
  const _Metric(this.label, this.value);
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
    decoration: BoxDecoration(
      color: Theme.of(context).colorScheme.surfaceContainerHigh,
      borderRadius: BorderRadius.circular(8),
    ),
    child: Text('$label: $value'),
  );
}

final class _ErrorView extends StatelessWidget {
  const _ErrorView({required this.error});
  final Object? error;

  @override
  Widget build(BuildContext context) => Center(
    child: Padding(
      padding: const EdgeInsets.all(24),
      child: SelectableText(
        'Demo error\n$error\n\nBuild and bundle the native bridge first.',
        textAlign: TextAlign.center,
      ),
    ),
  );
}
