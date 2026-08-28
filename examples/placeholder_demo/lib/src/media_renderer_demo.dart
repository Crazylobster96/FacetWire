// SPDX-License-Identifier: MPL-2.0
import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';
import 'package:path_provider/path_provider.dart';

import 'visual_transform.dart';

const mediaDemoDescriptor =
    'assets/documents/media-renderer-demo.agscene/'
    'media-renderer-demo.agscene.dis.json';
const _videoAsset =
    'assets/documents/media-renderer-demo.agscene/resources/'
    'facetwire-demo.mp4';
const _audioAsset =
    'assets/documents/media-renderer-demo.agscene/resources/'
    'facetwire-demo.wav';
const _videoPosterAsset =
    'assets/documents/media-renderer-demo.agscene/resources/'
    'video-poster.png';
const _audioArtworkAsset =
    'assets/documents/media-renderer-demo.agscene/resources/'
    'audio-artwork.png';

abstract class MediaDemoBackend extends ChangeNotifier {
  bool get ready;
  String? get error;
  bool get videoPlaying;
  bool get audioPlaying;
  Duration get videoPosition;
  Duration get videoDuration;
  Duration get audioPosition;
  Duration get audioDuration;

  Future<void> initialize();
  Widget buildVideoSurface();
  Future<void> toggleVideo();
  Future<void> toggleAudio();
  Future<void> seekVideo(Duration delta);
  Future<void> seekAudio(Duration delta);
  Future<void> setRate(double value);
  Future<void> close();
}

final class MediaKitDemoBackend extends MediaDemoBackend {
  MediaKitDemoBackend() : _videoPlayer = Player(), _audioPlayer = Player() {
    _videoController = VideoController(_videoPlayer);
  }

  final Player _videoPlayer;
  final Player _audioPlayer;
  late final VideoController _videoController;
  final List<StreamSubscription<dynamic>> _subscriptions = [];
  bool _ready = false;
  bool _closed = false;
  Future<void>? _initialization;
  String? _error;
  bool _videoPlaying = false;
  bool _audioPlaying = false;
  Duration _videoPosition = Duration.zero;
  Duration _videoDuration = Duration.zero;
  Duration _audioPosition = Duration.zero;
  Duration _audioDuration = Duration.zero;

  @override
  bool get ready => _ready;
  @override
  String? get error => _error;
  @override
  bool get videoPlaying => _videoPlaying;
  @override
  bool get audioPlaying => _audioPlaying;
  @override
  Duration get videoPosition => _videoPosition;
  @override
  Duration get videoDuration => _videoDuration;
  @override
  Duration get audioPosition => _audioPosition;
  @override
  Duration get audioDuration => _audioDuration;

  @override
  Future<void> initialize() {
    if (_closed) return Future<void>.value();
    return _initialization ??= _initialize();
  }

  Future<void> _initialize() async {
    if (_ready) return;
    try {
      _subscriptions.addAll([
        _videoPlayer.stream.playing.listen((value) {
          _videoPlaying = value;
          if (!_closed) notifyListeners();
        }),
        _videoPlayer.stream.position.listen((value) {
          _videoPosition = value;
          if (!_closed) notifyListeners();
        }),
        _videoPlayer.stream.duration.listen((value) {
          _videoDuration = value;
          if (!_closed) notifyListeners();
        }),
        _audioPlayer.stream.playing.listen((value) {
          _audioPlaying = value;
          if (!_closed) notifyListeners();
        }),
        _audioPlayer.stream.position.listen((value) {
          _audioPosition = value;
          if (!_closed) notifyListeners();
        }),
        _audioPlayer.stream.duration.listen((value) {
          _audioDuration = value;
          if (!_closed) notifyListeners();
        }),
        _videoPlayer.stream.error.listen(_recordError),
        _audioPlayer.stream.error.listen(_recordError),
      ]);
      final paths = await Future.wait([
        _stageAsset(_videoAsset, 'facetwire-demo.mp4'),
        _stageAsset(_audioAsset, 'facetwire-demo.wav'),
      ]);
      if (_closed) return;
      await _videoPlayer.setPlaylistMode(PlaylistMode.loop);
      await _audioPlayer.setPlaylistMode(PlaylistMode.loop);
      await _videoPlayer.open(Media(paths[0]), play: false);
      await _audioPlayer.open(Media(paths[1]), play: false);
      await _videoPlayer.setVolume(80);
      await _audioPlayer.setVolume(80);
      if (_closed) return;
      _ready = true;
      notifyListeners();
    } catch (exception) {
      _recordError(exception.toString());
    }
  }

  Future<String> _stageAsset(String asset, String fileName) async {
    final data = await rootBundle.load(asset);
    final temporary = await getTemporaryDirectory();
    final directory = Directory(
      '${temporary.path}${Platform.pathSeparator}'
      'facetwire-playground-media-v0.1',
    );
    await directory.create(recursive: true);
    final file = File('${directory.path}${Platform.pathSeparator}$fileName');
    await file.writeAsBytes(
      data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
      flush: true,
    );
    return file.path;
  }

  void _recordError(String value) {
    if (_closed) return;
    _error = value;
    notifyListeners();
  }

  @override
  Widget buildVideoSurface() => Video(
    controller: _videoController,
    controls: NoVideoControls,
    fit: BoxFit.contain,
    fill: Colors.transparent,
  );

  @override
  Future<void> toggleVideo() =>
      _videoPlaying ? _videoPlayer.pause() : _videoPlayer.play();

  @override
  Future<void> toggleAudio() =>
      _audioPlaying ? _audioPlayer.pause() : _audioPlayer.play();

  @override
  Future<void> seekVideo(Duration delta) => _videoPlayer.seek(
    _boundedPosition(_videoPosition, _videoDuration, delta),
  );

  @override
  Future<void> seekAudio(Duration delta) => _audioPlayer.seek(
    _boundedPosition(_audioPosition, _audioDuration, delta),
  );

  @override
  Future<void> setRate(double value) async {
    await _videoPlayer.setRate(value);
    await _audioPlayer.setRate(value);
  }

  static Duration _boundedPosition(
    Duration position,
    Duration duration,
    Duration delta,
  ) {
    final maximum = duration.inMilliseconds;
    final value = (position + delta).inMilliseconds.clamp(0, maximum).toInt();
    return Duration(milliseconds: value);
  }

  @override
  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    await _initialization;
    for (final subscription in _subscriptions) {
      await subscription.cancel();
    }
    _subscriptions.clear();
    await _videoPlayer.dispose();
    await _audioPlayer.dispose();
  }
}

class MediaRendererDemoScreen extends StatefulWidget {
  const MediaRendererDemoScreen({this.backend, super.key});

  final MediaDemoBackend? backend;

  @override
  State<MediaRendererDemoScreen> createState() =>
      _MediaRendererDemoScreenState();
}

class _MediaRendererDemoScreenState extends State<MediaRendererDemoScreen> {
  late final MediaDemoBackend _backend;
  double _videoOpacity = 0.92;
  double _subtitleOpacity = 0.96;
  double _controlsOpacity = 0.88;
  double _audioOpacity = 0.90;
  double _subtitleBottom = 64;
  double _rate = 1;
  int _contentQuarterTurns = 0;
  int _layerQuarterTurns = 0;
  bool _controlsVisible = true;

  @override
  void initState() {
    super.initState();
    _backend = widget.backend ?? MediaKitDemoBackend();
    unawaited(_backend.initialize());
  }

  @override
  void dispose() {
    unawaited(_disposeBackend());
    super.dispose();
  }

  Future<void> _disposeBackend() async {
    await _backend.close();
    _backend.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Audio/Video Renderer 0.1 Demo'),
        bottom: const PreferredSize(
          preferredSize: Size.fromHeight(24),
          child: Padding(
            padding: EdgeInsets.only(bottom: 6),
            child: Text('Video / Subtitle / Controls / Audio 独立 Layer'),
          ),
        ),
      ),
      body: AnimatedBuilder(
        animation: _backend,
        builder: (context, child) => LayoutBuilder(
          builder: (context, constraints) {
            final wide = constraints.maxWidth >= 1050;
            final preview = _buildPreview(scrollable: wide);
            final inspector = _buildInspector(scrollable: wide);
            return wide
                ? Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      Expanded(flex: 7, child: preview),
                      SizedBox(width: 360, child: inspector),
                    ],
                  )
                : ListView(
                    key: const ValueKey('media-compact-scroll'),
                    padding: const EdgeInsets.all(12),
                    children: [preview, inspector],
                  );
          },
        ),
      ),
    );
  }

  Widget _buildPreview({required bool scrollable}) {
    final content = Padding(
      padding: const EdgeInsets.all(20),
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 920),
        child: Column(
          children: [
            LayoutBuilder(
              builder: (context, constraints) {
                final landscapeWidth = math.min(constraints.maxWidth, 920.0);
                final landscapeHeight = landscapeWidth * 9 / 16;
                final layerIsPortrait = _layerQuarterTurns.isOdd;
                return SizedBox(
                  key: const ValueKey('media-video-viewport'),
                  width: layerIsPortrait ? landscapeHeight : landscapeWidth,
                  height: layerIsPortrait ? landscapeWidth : landscapeHeight,
                  child: _buildVideoComposition(),
                );
              },
            ),
            const SizedBox(height: 18),
            Opacity(opacity: _audioOpacity, child: _buildAudioLayer()),
          ],
        ),
      ),
    );
    return ColoredBox(
      color: const Color(0xff111827),
      child: Center(
        child: scrollable ? SingleChildScrollView(child: content) : content,
      ),
    );
  }

  Widget _buildVideoComposition() {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.black,
        border: Border.all(color: const Color(0xff60a5fa), width: 2),
        borderRadius: BorderRadius.circular(18),
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: Stack(
          fit: StackFit.expand,
          children: [
            const ColoredBox(
              key: ValueKey('media-video-background'),
              color: Colors.black,
            ),
            Opacity(
              key: const ValueKey('media-video-layer'),
              opacity: _videoOpacity,
              child:
                  VisualTransformSpec(
                    contentRotationQuarterTurns:
                        (_contentQuarterTurns + _layerQuarterTurns) % 4,
                  ).applyToVisual(
                    _backend.ready
                        ? _backend.buildVideoSurface()
                        : Image.asset(
                            _videoPosterAsset,
                            key: const ValueKey('media-video-poster'),
                            fit: BoxFit.contain,
                          ),
                    key: const ValueKey('media-video-transform'),
                  ),
            ),
            if (!_backend.ready && _backend.error == null)
              const Center(child: CircularProgressIndicator()),
            Positioned(
              left: 24,
              right: 24,
              bottom: _subtitleBottom,
              child: Opacity(
                key: const ValueKey('media-subtitle-layer'),
                opacity: _subtitleOpacity,
                child: Semantics(
                  label: '自动翻译字幕展示层',
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: Colors.black.withValues(alpha: 0.68),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: const Padding(
                      padding: EdgeInsets.symmetric(
                        horizontal: 14,
                        vertical: 8,
                      ),
                      child: Text(
                        '自动翻译字幕层 · 独立定位并贴合视频下方',
                        textAlign: TextAlign.center,
                        style: TextStyle(color: Colors.white, fontSize: 18),
                      ),
                    ),
                  ),
                ),
              ),
            ),
            Positioned(
              left: 22,
              right: 22,
              bottom: 12,
              child: IgnorePointer(
                ignoring: !_controlsVisible || _controlsOpacity == 0,
                child: AnimatedOpacity(
                  key: const ValueKey('media-controls-layer'),
                  opacity: _controlsVisible ? _controlsOpacity : 0,
                  duration: const Duration(milliseconds: 220),
                  child: _videoControls(),
                ),
              ),
            ),
            if (_backend.error case final error?)
              Align(
                alignment: Alignment.topCenter,
                child: Material(
                  color: const Color(0xff7f1d1d),
                  child: Padding(
                    padding: const EdgeInsets.all(8),
                    child: Text(
                      error,
                      key: const ValueKey('media-backend-error'),
                      style: const TextStyle(color: Colors.white),
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }

  Widget _videoControls() {
    return Material(
      color: const Color(0xdd111827),
      borderRadius: BorderRadius.circular(12),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          IconButton(
            key: const ValueKey('video-seek-back'),
            tooltip: '回退 2 秒',
            onPressed: _backend.ready
                ? () =>
                      unawaited(_backend.seekVideo(const Duration(seconds: -2)))
                : null,
            icon: const Icon(Icons.replay_10, color: Colors.white),
          ),
          IconButton(
            key: const ValueKey('video-play-pause'),
            tooltip: _backend.videoPlaying ? '暂停' : '播放',
            onPressed: _backend.ready
                ? () => unawaited(_backend.toggleVideo())
                : null,
            icon: Icon(
              _backend.videoPlaying ? Icons.pause : Icons.play_arrow,
              color: Colors.white,
            ),
          ),
          IconButton(
            key: const ValueKey('video-seek-forward'),
            tooltip: '快进 2 秒',
            onPressed: _backend.ready
                ? () =>
                      unawaited(_backend.seekVideo(const Duration(seconds: 2)))
                : null,
            icon: const Icon(Icons.forward_10, color: Colors.white),
          ),
          Expanded(
            child: Text(
              '${_format(_backend.videoPosition)} / '
              '${_format(_backend.videoDuration)}',
              textAlign: TextAlign.center,
              style: const TextStyle(color: Colors.white),
            ),
          ),
          IconButton(
            key: const ValueKey('video-rotate'),
            tooltip: '旋转视频内容 90°',
            onPressed: () => setState(
              () => _contentQuarterTurns = (_contentQuarterTurns + 1) % 4,
            ),
            icon: const Icon(Icons.rotate_90_degrees_cw, color: Colors.white),
          ),
          IconButton(
            key: const ValueKey('video-rotate-layer'),
            tooltip: '旋转视频层 90°',
            onPressed: () => setState(
              () => _layerQuarterTurns = (_layerQuarterTurns + 1) % 4,
            ),
            icon: const Icon(Icons.crop_rotate, color: Colors.white),
          ),
        ],
      ),
    );
  }

  Widget _buildAudioLayer() {
    return Material(
      key: const ValueKey('media-audio-layer'),
      color: const Color(0xffe0e7ff),
      borderRadius: BorderRadius.circular(18),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Row(
          children: [
            ClipRRect(
              borderRadius: BorderRadius.circular(12),
              child: Image.asset(
                _audioArtworkAsset,
                width: 112,
                height: 112,
                fit: BoxFit.cover,
              ),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'FacetWire Theme · Audio Layer',
                    style: TextStyle(fontWeight: FontWeight.w700, fontSize: 18),
                  ),
                  Text(
                    '${_format(_backend.audioPosition)} / '
                    '${_format(_backend.audioDuration)}',
                  ),
                  Row(
                    children: [
                      IconButton(
                        key: const ValueKey('audio-seek-back'),
                        onPressed: _backend.ready
                            ? () => unawaited(
                                _backend.seekAudio(const Duration(seconds: -2)),
                              )
                            : null,
                        icon: const Icon(Icons.replay_10),
                      ),
                      IconButton(
                        key: const ValueKey('audio-play-pause'),
                        onPressed: _backend.ready
                            ? () => unawaited(_backend.toggleAudio())
                            : null,
                        icon: Icon(
                          _backend.audioPlaying
                              ? Icons.pause_circle
                              : Icons.play_circle,
                        ),
                      ),
                      IconButton(
                        key: const ValueKey('audio-seek-forward'),
                        onPressed: _backend.ready
                            ? () => unawaited(
                                _backend.seekAudio(const Duration(seconds: 2)),
                              )
                            : null,
                        icon: const Icon(Icons.forward_10),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildInspector({required bool scrollable}) {
    return Material(
      color: Theme.of(context).colorScheme.surfaceContainerLow,
      child: ListView(
        key: const ValueKey('media-layer-inspector'),
        padding: const EdgeInsets.all(16),
        primary: false,
        physics: scrollable ? null : const NeverScrollableScrollPhysics(),
        shrinkWrap: true,
        children: [
          const Text(
            'Layer Inspector / 图层检查器',
            style: TextStyle(fontWeight: FontWeight.w700, fontSize: 20),
          ),
          const SizedBox(height: 4),
          const SelectableText(mediaDemoDescriptor),
          _layerSlider('Video 不透明度', 'media-video-opacity', _videoOpacity, (
            value,
          ) {
            setState(() => _videoOpacity = value);
          }),
          _layerSlider(
            'Subtitle 不透明度',
            'media-subtitle-opacity',
            _subtitleOpacity,
            (value) => setState(() => _subtitleOpacity = value),
          ),
          _layerSlider(
            'Controls 不透明度',
            'media-controls-opacity',
            _controlsOpacity,
            (value) => setState(() => _controlsOpacity = value),
          ),
          _layerSlider('Audio 不透明度', 'media-audio-opacity', _audioOpacity, (
            value,
          ) {
            setState(() => _audioOpacity = value);
          }),
          _layerSlider(
            '字幕距底部',
            'media-subtitle-position',
            _subtitleBottom,
            (value) => setState(() => _subtitleBottom = value),
            minimum: 48,
            maximum: 180,
            divisions: 22,
          ),
          SwitchListTile(
            key: const ValueKey('media-controls-visible'),
            value: _controlsVisible,
            onChanged: (value) => setState(() => _controlsVisible = value),
            title: const Text('显示控制 Layer'),
            subtitle: const Text('关闭时渐隐且不接收输入'),
          ),
          const SizedBox(height: 8),
          SegmentedButton<double>(
            key: const ValueKey('media-playback-rate'),
            segments: const [
              ButtonSegment(value: 0.5, label: Text('0.5×')),
              ButtonSegment(value: 1, label: Text('1×')),
              ButtonSegment(value: 1.5, label: Text('1.5×')),
              ButtonSegment(value: 2, label: Text('2×')),
            ],
            selected: {_rate},
            onSelectionChanged: (selection) {
              final value = selection.single;
              setState(() => _rate = value);
              unawaited(_backend.setRate(value));
            },
          ),
          const SizedBox(height: 16),
          Text(
            '内容旋转 ${_contentQuarterTurns * 90}° · '
            '视频层旋转 ${_layerQuarterTurns * 90}°',
            key: const ValueKey('media-rotation-status'),
          ),
          const SizedBox(height: 8),
          Text(
            _backend.ready
                ? 'Media Service ready · local MP4 + WAV'
                : 'Preparing local Media Service…',
            key: const ValueKey('media-backend-status'),
          ),
          const SizedBox(height: 8),
          const Text('不透明度统一：1 = 完全不透明，0 = 完全透明。'),
        ],
      ),
    );
  }

  Widget _layerSlider(
    String label,
    String keyName,
    double value,
    ValueChanged<double> onChanged, {
    double minimum = 0,
    double maximum = 1,
    int divisions = 100,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('$label：${value.toStringAsFixed(2)}'),
        Slider(
          key: ValueKey(keyName),
          value: value,
          min: minimum,
          max: maximum,
          divisions: divisions,
          onChanged: onChanged,
        ),
      ],
    );
  }

  static String _format(Duration value) {
    final seconds = value.inSeconds.remainder(60).toString().padLeft(2, '0');
    return '${value.inMinutes}:$seconds';
  }
}
