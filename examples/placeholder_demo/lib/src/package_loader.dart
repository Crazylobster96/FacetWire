// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:flutter/services.dart';

import 'demo_models.dart';

typedef AssetTextReader = Future<String> Function(String path);
typedef AssetProbe = Future<void> Function(String path);

final class SceneBounds {
  const SceneBounds(this.x, this.y, this.width, this.height);
  final double x;
  final double y;
  final double width;
  final double height;
}

enum SceneDocumentFit {
  none,
  contain,
  cover,
  fill;

  static SceneDocumentFit fromDocument(String? value) => switch (value) {
    null || 'none' => none,
    'contain' => contain,
    'cover' => cover,
    'fill' => fill,
    _ => throw FormatException('Unsupported document placement fit: $value'),
  };
}

final class SceneDocumentPlacement {
  const SceneDocumentPlacement({
    required this.fit,
    required this.alignmentX,
    required this.alignmentY,
    required this.clip,
  });

  static const defaults = SceneDocumentPlacement(
    fit: SceneDocumentFit.none,
    alignmentX: 0,
    alignmentY: 0,
    clip: false,
  );

  final SceneDocumentFit fit;
  final double alignmentX;
  final double alignmentY;
  final bool clip;
}

final class SceneZone {
  const SceneZone({
    required this.id,
    required this.bounds,
    required this.contentType,
    required this.kind,
    required this.reason,
    required this.mode,
    required this.label,
    required this.permittedActions,
    required this.child,
    required this.documentPlacement,
  });

  final String id;
  final SceneBounds bounds;
  final String contentType;
  final String kind;
  final PlaceholderReason reason;
  final PlaceholderMode mode;
  final String label;
  final int permittedActions;
  final SceneDocument? child;
  final SceneDocumentPlacement? documentPlacement;
}

final class SceneLayer {
  const SceneLayer(this.id, this.z, this.zones);
  final String id;
  final int z;
  final List<SceneZone> zones;
}

final class ScenePage {
  const ScenePage(this.id, this.width, this.height, this.layers);
  final String id;
  final double width;
  final double height;
  final List<SceneLayer> layers;
}

final class SceneDocument {
  const SceneDocument({
    required this.id,
    required this.title,
    required this.descriptorPath,
    required this.canvasId,
    required this.canvasWidth,
    required this.canvasHeight,
    required this.pages,
    required this.resourceCount,
    required this.depth,
  });

  final String id;
  final String title;
  final String descriptorPath;
  final String canvasId;
  final double canvasWidth;
  final double canvasHeight;
  final List<ScenePage> pages;
  final int resourceCount;
  final int depth;

  Iterable<SceneZone> get zones sync* {
    for (final page in pages) {
      for (final layer in page.layers) {
        yield* layer.zones;
      }
    }
  }

  Iterable<SceneDocument> get documents sync* {
    yield this;
    for (final zone in zones) {
      final child = zone.child;
      if (child != null) {
        yield* child.documents;
      }
    }
  }

  Iterable<SceneZone> get placeholderZones sync* {
    for (final zone in zones) {
      if (zone.contentType != 'document') {
        yield zone;
      }
      final child = zone.child;
      if (child != null) {
        yield* child.placeholderZones;
      }
    }
  }
}

final class AgscenePackageLoader {
  const AgscenePackageLoader(this._readText, {this.maxDepth = 32, this.probe});

  factory AgscenePackageLoader.asset({int maxDepth = 32}) =>
      AgscenePackageLoader(
        rootBundle.loadString,
        maxDepth: maxDepth,
        probe: (path) async {
          await rootBundle.load(path);
        },
      );

  final AssetTextReader _readText;
  final AssetProbe? probe;
  final int maxDepth;

  Future<SceneDocument> load(String descriptorPath) =>
      _load(_normalizeRoot(descriptorPath), 1, <String>{});

  Future<SceneDocument> _load(
    String descriptorPath,
    int depth,
    Set<String> ancestors,
  ) async {
    if (depth > maxDepth) {
      throw FormatException('Document recursion exceeds depth $maxDepth.');
    }
    _validateDescriptorName(descriptorPath);
    if (!ancestors.add(descriptorPath)) {
      throw FormatException('Document cycle detected at $descriptorPath.');
    }
    try {
      final source = await _readText(descriptorPath);
      final value = jsonDecode(source);
      if (value is! Map<String, Object?>) {
        throw const FormatException('Descriptor root must be an object.');
      }
      _expect(value['format'] == 'facetwire.agent-scene-package', 'format');
      _expect(value['version'] == '0.1', 'version');
      final id = _requiredString(value, 'id');
      final title = _requiredString(value, 'title');
      final canvas = _requiredMap(value, 'canvas');
      final canvasId = _requiredString(canvas, 'id');
      final canvasSize = _requiredMap(canvas, 'size');
      final canvasWidth = _nonNegativeNumber(canvasSize, 'width');
      final canvasHeight = _nonNegativeNumber(canvasSize, 'height');
      final pagesJson = _requiredList(canvas, 'pages');
      _expect(pagesJson.isNotEmpty, 'canvas.pages');
      final seenIds = <String>{id, canvasId};
      final packageRoot = _directoryOf(descriptorPath);
      final pages = <ScenePage>[];
      for (final pageValue in pagesJson) {
        final page = _asMap(pageValue, 'page');
        final pageId = _uniqueId(page, seenIds);
        final pageSize = _requiredMap(page, 'size');
        final layers = <SceneLayer>[];
        for (final layerValue in _requiredList(page, 'layers')) {
          final layer = _asMap(layerValue, 'layer');
          final layerId = _uniqueId(layer, seenIds);
          final z = layer['z'];
          _expect(z is int, '$layerId.z');
          final zones = <SceneZone>[];
          for (final zoneValue in _requiredList(layer, 'zones')) {
            final zone = _asMap(zoneValue, 'zone');
            final zoneId = _uniqueId(zone, seenIds);
            final bounds = _requiredMap(zone, 'bounds');
            final content = _requiredMap(zone, 'content');
            final contentType = _requiredString(content, 'type');
            SceneDocument? child;
            var kind = 'document';
            var reason = PlaceholderReason.unknown;
            var mode = PlaceholderMode.standard;
            var label = zoneId;
            var permittedActions = 0;
            SceneDocumentPlacement? documentPlacement;
            if (contentType == 'document') {
              final relative = _portableRelativePath(
                _requiredString(content, 'source'),
              );
              child = await _load(
                '$packageRoot/$relative',
                depth + 1,
                ancestors,
              );
              documentPlacement = _documentPlacement(content, zoneId);
              label = child.title;
            } else if (contentType == 'placeholder') {
              kind = _requiredString(content, 'kind');
              reason = PlaceholderReason.fromDocument(
                _requiredString(content, 'reason'),
              );
              mode = PlaceholderMode.fromDocument(
                _requiredString(content, 'mode'),
              );
              label = content['label'] is String
                  ? content['label']! as String
                  : zoneId;
              final actions = content['permittedActions'];
              if (actions != null) {
                _expect(actions is List<Object?>, '$zoneId.permittedActions');
                permittedActions = PlaceholderAction.fromNames(
                  (actions as List<Object?>).cast<String>(),
                );
              }
            } else {
              kind = contentType;
              reason = PlaceholderReason.unsupportedType;
              mode = PlaceholderMode.standard;
              label = 'Unsupported content type: $contentType';
              permittedActions =
                  PlaceholderAction.findPlugin | PlaceholderAction.alternative;
            }
            zones.add(
              SceneZone(
                id: zoneId,
                bounds: SceneBounds(
                  _finiteNumber(bounds, 'x'),
                  _finiteNumber(bounds, 'y'),
                  _nonNegativeNumber(bounds, 'width'),
                  _nonNegativeNumber(bounds, 'height'),
                ),
                contentType: contentType,
                kind: kind,
                reason: reason,
                mode: mode,
                label: label,
                permittedActions: permittedActions,
                child: child,
                documentPlacement: documentPlacement,
              ),
            );
          }
          layers.add(SceneLayer(layerId, z! as int, zones));
        }
        layers.sort((left, right) => left.z.compareTo(right.z));
        pages.add(
          ScenePage(
            pageId,
            _nonNegativeNumber(pageSize, 'width'),
            _nonNegativeNumber(pageSize, 'height'),
            layers,
          ),
        );
      }
      final resources = _requiredList(value, 'resources');
      for (final resourceValue in resources) {
        final resource = _asMap(resourceValue, 'resource');
        _uniqueId(resource, seenIds);
        _requiredString(resource, 'mediaType');
        final relative = _portableRelativePath(
          _requiredString(resource, 'source'),
        );
        final resourcePath = '$packageRoot/$relative';
        final resourceProbe = probe;
        if (resourceProbe == null) {
          await _readText(resourcePath);
        } else {
          await resourceProbe(resourcePath);
        }
      }
      return SceneDocument(
        id: id,
        title: title,
        descriptorPath: descriptorPath,
        canvasId: canvasId,
        canvasWidth: canvasWidth,
        canvasHeight: canvasHeight,
        pages: pages,
        resourceCount: resources.length,
        depth: depth,
      );
    } finally {
      ancestors.remove(descriptorPath);
    }
  }

  static String _normalizeRoot(String value) {
    if (value.startsWith('/') || value.contains('\\') || value.contains('//')) {
      throw FormatException('Unsafe descriptor path: $value');
    }
    return value;
  }

  static String _portableRelativePath(String value) {
    if (value.isEmpty ||
        value.startsWith('/') ||
        RegExp(r'^[A-Za-z]:').hasMatch(value) ||
        value.contains('\\') ||
        value.contains('//') ||
        value.split('/').any((segment) => segment == '.' || segment == '..')) {
      throw FormatException('Unsafe package-relative path: $value');
    }
    return value;
  }

  static void _validateDescriptorName(String path) {
    final segments = path.split('/');
    _expect(segments.length >= 2, 'descriptor path');
    final directoryName = segments[segments.length - 2];
    final fileName = segments.last;
    _expect(directoryName.endsWith('.agscene'), 'package suffix');
    _expect(fileName == '$directoryName.dis.json', 'descriptor file name');
  }

  static String _directoryOf(String path) =>
      path.substring(0, path.lastIndexOf('/'));

  static Map<String, Object?> _asMap(Object? value, String field) {
    if (value is! Map<String, Object?>) {
      throw FormatException('$field must be an object.');
    }
    return value;
  }

  static Map<String, Object?> _requiredMap(
    Map<String, Object?> value,
    String field,
  ) => _asMap(value[field], field);

  static List<Object?> _requiredList(Map<String, Object?> value, String field) {
    final result = value[field];
    if (result is! List<Object?>) {
      throw FormatException('$field must be an array.');
    }
    return result;
  }

  static String _requiredString(Map<String, Object?> value, String field) {
    final result = value[field];
    if (result is! String || result.isEmpty) {
      throw FormatException('$field must be a non-empty string.');
    }
    return result;
  }

  static String _uniqueId(Map<String, Object?> value, Set<String> seenIds) {
    final id = _requiredString(value, 'id');
    if (!seenIds.add(id)) {
      throw FormatException('Duplicate ID in document: $id');
    }
    return id;
  }

  static double _finiteNumber(Map<String, Object?> value, String field) {
    final result = value[field];
    if (result is! num || !result.toDouble().isFinite) {
      throw FormatException('$field must be finite.');
    }
    return result.toDouble();
  }

  static double _nonNegativeNumber(Map<String, Object?> value, String field) {
    final result = _finiteNumber(value, field);
    if (result < 0) {
      throw FormatException('$field must be non-negative.');
    }
    return result;
  }

  static SceneDocumentPlacement _documentPlacement(
    Map<String, Object?> content,
    String zoneId,
  ) {
    final value = content['placement'];
    if (value == null) return SceneDocumentPlacement.defaults;
    final placement = _asMap(value, '$zoneId.content.placement');
    final fitValue = placement['fit'];
    _expect(
      fitValue == null || fitValue is String,
      '$zoneId.content.placement.fit',
    );
    final alignmentValue = placement['alignment'];
    var alignmentX = 0.0;
    var alignmentY = 0.0;
    if (alignmentValue != null) {
      final alignment = _asMap(
        alignmentValue,
        '$zoneId.content.placement.alignment',
      );
      alignmentX = _unitNumber(
        alignment,
        'x',
        '$zoneId.content.placement.alignment.x',
      );
      alignmentY = _unitNumber(
        alignment,
        'y',
        '$zoneId.content.placement.alignment.y',
      );
    }
    final clipValue = placement['clip'];
    _expect(
      clipValue == null || clipValue is bool,
      '$zoneId.content.placement.clip',
    );
    return SceneDocumentPlacement(
      fit: SceneDocumentFit.fromDocument(fitValue as String?),
      alignmentX: alignmentX,
      alignmentY: alignmentY,
      clip: clipValue as bool? ?? false,
    );
  }

  static double _unitNumber(
    Map<String, Object?> value,
    String field,
    String diagnostic,
  ) {
    final result = value[field];
    if (result is! num || !result.toDouble().isFinite) {
      throw FormatException('$diagnostic must be finite.');
    }
    final number = result.toDouble();
    if (number < 0 || number > 1) {
      throw FormatException('$diagnostic must be between 0 and 1.');
    }
    return number;
  }

  static void _expect(bool condition, String field) {
    if (!condition) {
      throw FormatException('Invalid $field.');
    }
  }
}
