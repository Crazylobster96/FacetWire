// SPDX-License-Identifier: MPL-2.0
import 'dart:convert';

import 'package:facetwire_placeholder_demo/src/media_renderer_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

final class _FakeMediaBackend extends MediaDemoBackend {
  _FakeMediaBackend({this.readyOnInitialize = true});

  final bool readyOnInitialize;
  bool _ready = false;
  bool _videoPlaying = false;
  bool _audioPlaying = false;
  bool closed = false;
  int videoToggles = 0;
  int audioToggles = 0;
  final List<Duration> videoSeeks = [];
  final List<Duration> audioSeeks = [];
  double rate = 1;

  @override
  bool get ready => _ready;
  @override
  String? get error => null;
  @override
  bool get videoPlaying => _videoPlaying;
  @override
  bool get audioPlaying => _audioPlaying;
  @override
  Duration get videoPosition => const Duration(seconds: 2);
  @override
  Duration get videoDuration => const Duration(seconds: 6);
  @override
  Duration get audioPosition => const Duration(seconds: 3);
  @override
  Duration get audioDuration => const Duration(seconds: 6);

  @override
  Future<void> initialize() async {
    if (readyOnInitialize) markReady();
  }

  void markReady() {
    _ready = true;
    notifyListeners();
  }

  @override
  Widget buildVideoSurface() => const ColoredBox(
    key: ValueKey('fake-video-surface'),
    color: Colors.indigo,
  );

  @override
  Future<void> toggleVideo() async {
    videoToggles += 1;
    _videoPlaying = !_videoPlaying;
    notifyListeners();
  }

  @override
  Future<void> toggleAudio() async {
    audioToggles += 1;
    _audioPlaying = !_audioPlaying;
    notifyListeners();
  }

  @override
  Future<void> seekVideo(Duration delta) async => videoSeeks.add(delta);

  @override
  Future<void> seekAudio(Duration delta) async => audioSeeks.add(delta);

  @override
  Future<void> setRate(double value) async => rate = value;

  @override
  Future<void> close() async => closed = true;
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test(
    'media demo package declares local video audio poster and artwork',
    () async {
      final json = jsonDecode(
        await rootBundle.loadString(mediaDemoDescriptor),
      ) as Map<String, dynamic>;
      final resources = (json['resources'] as List<dynamic>)
          .cast<Map<String, dynamic>>();
      expect(json['format'], 'facetwire.agent-scene-package');
      expect(
        resources.map((resource) => resource['mediaType']),
        containsAll(['video/mp4', 'audio/wav', 'image/png']),
      );
      expect(
        resources.map((resource) => resource['source']),
        containsAll([
          'resources/facetwire-demo.mp4',
          'resources/facetwire-demo.wav',
          'resources/video-poster.png',
          'resources/audio-artwork.png',
        ]),
      );
    },
  );

  testWidgets('keeps media subtitle controls and audio as independent layers', (
    tester,
  ) async {
    final backend = _FakeMediaBackend();
    await tester.binding.setSurfaceSize(const Size(1400, 900));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(
      MaterialApp(home: MediaRendererDemoScreen(backend: backend)),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const ValueKey('fake-video-surface')), findsOneWidget);
    expect(find.byKey(const ValueKey('media-video-layer')), findsOneWidget);
    expect(find.byKey(const ValueKey('media-subtitle-layer')), findsOneWidget);
    expect(find.byKey(const ValueKey('media-controls-layer')), findsOneWidget);
    expect(find.byKey(const ValueKey('media-audio-layer')), findsOneWidget);

    final initialViewport = tester.getSize(
      find.byKey(const ValueKey('media-video-viewport')),
    );
    expect(initialViewport.width, closeTo(920, 0.01));
    expect(initialViewport.height, closeTo(517.5, 0.01));
    expect(
      tester
          .widget<RotatedBox>(
            find.byKey(const ValueKey('media-video-transform')),
          )
          .quarterTurns,
      0,
    );

    await tester.tap(find.byKey(const ValueKey('video-rotate')));
    await tester.pump();
    expect(
      tester
          .widget<RotatedBox>(
            find.byKey(const ValueKey('media-video-transform')),
          )
          .quarterTurns,
      1,
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('media-video-viewport'))),
      initialViewport,
    );

    await tester.tap(find.byKey(const ValueKey('video-play-pause')));
    await tester.tap(find.byKey(const ValueKey('video-seek-forward')));
    await tester.tap(find.byKey(const ValueKey('audio-play-pause')));
    await tester.tap(find.byKey(const ValueKey('audio-seek-back')));
    await tester.pump();
    expect(backend.videoToggles, 1);
    expect(backend.videoSeeks, [const Duration(seconds: 2)]);
    expect(backend.audioToggles, 1);
    expect(backend.audioSeeks, [const Duration(seconds: -2)]);

    final opacity = find.byKey(const ValueKey('media-video-opacity'));
    await tester.drag(opacity, const Offset(-120, 0));
    await tester.pump();
    expect(tester.widget<Slider>(opacity).value, lessThan(0.92));

    await tester.tap(find.text('1.5×'));
    await tester.pump();
    expect(backend.rate, 1.5);

    await tester.tap(find.byKey(const ValueKey('media-controls-visible')));
    await tester.pump(const Duration(milliseconds: 250));
    expect(
      tester
          .widget<AnimatedOpacity>(
            find.byKey(const ValueKey('media-controls-layer')),
          )
          .opacity,
      0,
    );

    await tester.pumpWidget(const SizedBox.shrink());
    await tester.pump();
    expect(backend.closed, isTrue);
  });

  testWidgets('layer rotation swaps zone size and reflows related layers', (
    tester,
  ) async {
    final backend = _FakeMediaBackend();
    await tester.binding.setSurfaceSize(const Size(1400, 1100));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(
      MaterialApp(home: MediaRendererDemoScreen(backend: backend)),
    );
    await tester.pumpAndSettle();

    final viewport = find.byKey(const ValueKey('media-video-viewport'));
    final initialViewport = tester.getSize(viewport);
    await tester.tap(find.byKey(const ValueKey('video-rotate-layer')));
    await tester.pump();

    final portraitViewport = tester.getSize(viewport);
    expect(portraitViewport.width, closeTo(initialViewport.height, 0.01));
    expect(portraitViewport.height, closeTo(initialViewport.width, 0.01));
    expect(find.byKey(const ValueKey('media-subtitle-layer')), findsOneWidget);
    expect(find.byKey(const ValueKey('media-controls-layer')), findsOneWidget);
    expect(
      tester
          .widget<RotatedBox>(
            find.byKey(const ValueKey('media-video-transform')),
          )
          .quarterTurns,
      1,
    );

    await tester.pumpWidget(const SizedBox.shrink());
    await tester.pump();
  });

  testWidgets('poster and live video share one visual slot', (tester) async {
    final backend = _FakeMediaBackend(readyOnInitialize: false);
    await tester.binding.setSurfaceSize(const Size(1400, 900));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(
      MaterialApp(home: MediaRendererDemoScreen(backend: backend)),
    );
    await tester.pump();

    expect(find.byKey(const ValueKey('media-video-poster')), findsOneWidget);
    expect(find.byKey(const ValueKey('fake-video-surface')), findsNothing);

    backend.markReady();
    await tester.pump();

    expect(find.byKey(const ValueKey('media-video-poster')), findsNothing);
    expect(find.byKey(const ValueKey('fake-video-surface')), findsOneWidget);

    await tester.pumpWidget(const SizedBox.shrink());
    await tester.pump();
  });
}
