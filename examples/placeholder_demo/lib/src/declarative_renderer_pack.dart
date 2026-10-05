// SPDX-License-Identifier: MPL-2.0
// Bounded, data-only runtime renderer profile for trusted desktop hosts.
// This is not executable Dart/native plugin loading or a sandbox.
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/material.dart';

import 'core_content_demo.dart';

const _schema = 'facetwire.declarative-zone.v1';
const _maxPackBytes = 65536;
const _maxNodes = 64;
const _maxDepth = 8;
const _builtInTypes = {
  'text',
  'image',
  'animated-image',
  'video',
  'audio',
  'chart',
  'document',
};

final class DeclarativeRendererPack {
  DeclarativeRendererPack._(this.type, this._view);

  final String type;
  final Map<String, Object?> _view;

  /// The caller must explicitly select a trusted local pack. The file itself
  /// cannot authorize model calls, tools, network access or task execution.
  static Future<DeclarativeRendererPack> load(
    String absolutePath, {
    required String expectedSha256,
  }) async {
    if (!RegExp(r'^[0-9a-f]{64}$').hasMatch(expectedSha256)) {
      throw const FormatException('An exact renderer pack SHA-256 is required');
    }
    final file = File(absolutePath);
    final absolute = Platform.isWindows
        ? RegExp(r'^[A-Za-z]:[\\/]').hasMatch(absolutePath) ||
              absolutePath.startsWith(r'\\')
        : absolutePath.startsWith('/');
    if (!absolute ||
        await FileSystemEntity.type(absolutePath, followLinks: false) !=
            FileSystemEntityType.file) {
      throw const FormatException(
        'A regular absolute renderer pack file is required',
      );
    }
    final length = await file.length();
    if (length < 1 || length > _maxPackBytes) {
      throw const FormatException('Renderer pack exceeds its byte limit');
    }
    final bytes = await file.readAsBytes();
    if (bytes.length != length) {
      throw const FormatException('Renderer pack changed during read');
    }
    if (sha256.convert(bytes).toString() != expectedSha256) {
      throw const FormatException('Renderer pack SHA-256 changed');
    }
    return parse(bytes);
  }

  static DeclarativeRendererPack parse(List<int> bytes) {
    if (bytes.isEmpty || bytes.length > _maxPackBytes) {
      throw const FormatException('Renderer pack exceeds its byte limit');
    }
    final Object? decoded;
    try {
      decoded = jsonDecode(utf8.decode(bytes, allowMalformed: false));
    } on FormatException {
      throw const FormatException('Renderer pack must be strict UTF-8 JSON');
    }
    final root = _object(decoded, 'renderer pack');
    _keys(root, {'schema', 'type', 'view'}, 'renderer pack');
    if (root['schema'] != _schema ||
        root['type'] is! String ||
        !RegExp(r'^[a-z][a-z0-9-]{0,63}$').hasMatch(root['type']! as String) ||
        _builtInTypes.contains(root['type'])) {
      throw const FormatException('Unsupported renderer pack identity');
    }
    var nodes = 0;
    final view = _viewNode(root['view'], 1, () => ++nodes);
    if (nodes > _maxNodes) {
      throw const FormatException('Renderer pack has too many view nodes');
    }
    // A trusted installer must pin the file's cryptographic digest before it
    // is passed here; parsing a path is not a trust decision.
    return DeclarativeRendererPack._(root['type']! as String, view);
  }

  FacetWireZoneRenderer asRenderer() => FacetWireZoneRenderer(
    type: type,
    validate: (content, resources) {
      if (content['type'] != type || resources.length > 256) {
        throw const FormatException('Zone does not match renderer pack');
      }
      final fields = <String>{};
      _collectFields(_view, fields);
      for (final key in content.keys) {
        if (key != 'type' && key != 'opacity' && !fields.contains(key)) {
          throw FormatException('Unrecognized renderer content field: $key');
        }
      }
      for (final field in fields) {
        final value = content[field];
        if (value is! String ||
            value.isEmpty ||
            utf8.encode(value).length > 2048) {
          throw FormatException('Renderer content field unavailable: $field');
        }
      }
      _checkContent(_view, content);
    },
    build: (context, zone) => _build(_view, zone.content),
  );
}

Map<String, Object?> _object(Object? value, String name) {
  if (value is! Map<String, dynamic>) {
    throw FormatException('$name must be an object');
  }
  return Map<String, Object?>.from(value);
}

void _keys(Map<String, Object?> value, Set<String> allowed, String name) {
  if (value.keys.toSet().difference(allowed).isNotEmpty ||
      allowed.difference(value.keys.toSet()).isNotEmpty) {
    throw FormatException('$name has missing or unsupported fields');
  }
}

Map<String, Object?> _viewNode(
  Object? source,
  int depth,
  int Function() count,
) {
  if (depth > _maxDepth || count() > _maxNodes) {
    throw const FormatException('Renderer view depth or node limit exceeded');
  }
  final node = _object(source, 'view node');
  switch (node['kind']) {
    case 'text':
      _keys(node, {'kind', 'field', 'colorField'}, 'text view');
      if (!_field(node['field']) || !_field(node['colorField'])) {
        throw const FormatException('Text view needs valid content fields');
      }
    case 'box':
      _keys(node, {'kind', 'colorField', 'child'}, 'box view');
      if (!_field(node['colorField'])) {
        throw const FormatException('Box view needs a color field');
      }
      node['child'] = _viewNode(node['child'], depth + 1, count);
    case 'column':
      _keys(node, {'kind', 'children'}, 'column view');
      final children = node['children'];
      if (children is! List || children.isEmpty || children.length > 16) {
        throw const FormatException('Column view needs 1–16 children');
      }
      node['children'] = children
          .map((child) => _viewNode(child, depth + 1, count))
          .toList(growable: false);
    default:
      throw const FormatException('Unknown renderer view primitive');
  }
  return Map<String, Object?>.unmodifiable(node);
}

bool _field(Object? value) =>
    value is String &&
    RegExp(r'^[a-z][A-Za-z0-9]{0,63}$').hasMatch(value) &&
    value != 'type';

void _collectFields(Map<String, Object?> node, Set<String> fields) {
  switch (node['kind']) {
    case 'text':
      fields.add(node['field']! as String);
      fields.add(node['colorField']! as String);
    case 'box':
      fields.add(node['colorField']! as String);
      _collectFields(node['child']! as Map<String, Object?>, fields);
    case 'column':
      for (final child in node['children']! as List<Map<String, Object?>>) {
        _collectFields(child, fields);
      }
  }
}

Color _color(String value) {
  if (!RegExp(r'^#[0-9a-fA-F]{8}$').hasMatch(value)) {
    throw const FormatException('Renderer color needs #RRGGBBAA');
  }
  final raw = int.parse(value.substring(1), radix: 16);
  return Color(((raw & 0xff) << 24) | (raw >>> 8));
}

void _checkContent(Map<String, Object?> node, Map<String, Object?> content) {
  switch (node['kind']) {
    case 'text':
      _color(content[node['colorField']]! as String);
    case 'box':
      _color(content[node['colorField']]! as String);
      _checkContent(node['child']! as Map<String, Object?>, content);
    case 'column':
      for (final child in node['children']! as List<Map<String, Object?>>) {
        _checkContent(child, content);
      }
  }
}

Widget _build(Map<String, Object?> node, Map<String, Object?> content) {
  switch (node['kind']) {
    case 'text':
      return Text(
        content[node['field']]! as String,
        style: TextStyle(color: _color(content[node['colorField']]! as String)),
      );
    case 'box':
      return ColoredBox(
        color: _color(content[node['colorField']]! as String),
        child: _build(node['child']! as Map<String, Object?>, content),
      );
    case 'column':
      return Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          for (final child in node['children']! as List<Map<String, Object?>>)
            _build(child, content),
        ],
      );
    default:
      throw const FormatException('Validated renderer view changed');
  }
}
