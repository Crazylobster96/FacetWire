// SPDX-License-Identifier: MPL-2.0
import 'package:facetwire_placeholder_demo/src/demo_app.dart';
import 'package:facetwire_placeholder_demo/src/demo_models.dart';
import 'package:facetwire_placeholder_demo/src/package_loader.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('shows recursive scene and real-contract controls', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1500, 900));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final client = _FakeClient();

    await tester.pumpWidget(
      PlaceholderDemoApp(
        client: client,
        packageLoader: AgscenePackageLoader.asset(),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('3 recursive document levels'), findsOneWidget);
    expect(find.text('Placeholder Demo / Level 1'), findsOneWidget);
    expect(find.text('Nested Document / Level 2'), findsOneWidget);
    expect(find.text('Nested Document / Level 3'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('composed-scene-preview')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('scene-100-percent-viewport')),
      findsOneWidget,
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('root-canvas-surface'))),
      const Size(1900, 1200),
    );

    final root = find.byKey(const ValueKey('root-canvas-surface'));
    final level2 = find.byKey(const ValueKey('scene-zone:zone:nested-level-2'));
    final level3 = find.byKey(const ValueKey('scene-zone:zone:nested-level-3'));
    final video = find.byKey(const ValueKey('scene-zone:zone:video'));

    expect(
      tester.getTopLeft(level2) - tester.getTopLeft(root),
      const Offset(746, 70),
    );
    expect(tester.getSize(level2), const Size(1100, 1080));
    expect(
      tester.getTopLeft(level3) - tester.getTopLeft(level2),
      const Offset(36, 448),
    );
    expect(tester.getSize(level3), const Size(1024, 576));
    expect(
      tester.getTopLeft(video) - tester.getTopLeft(level3),
      const Offset(48, 48),
    );
    expect(tester.getSize(video), const Size(928, 480));
    expect(
      tester.getTopLeft(video) - tester.getTopLeft(root),
      const Offset(830, 566),
    );
    expect(find.text('zone:nested-level-2'), findsOneWidget);
    expect(find.text('zone:nested-level-3'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('placeholder-display-list:zone:hero-image')),
      findsOneWidget,
    );
    expect(find.text('canvas:level-1 → zone:hero-image'), findsOneWidget);
    expect(find.textContaining('Final background alpha'), findsOneWidget);
    expect(client.renderCalls, greaterThanOrEqualTo(3));
  });

  testWidgets('uses tabbed workspace without overflow at phone size', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(430, 932));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(
      PlaceholderDemoApp(
        key: const ValueKey('phone-sized-demo'),
        client: _FakeClient(),
        packageLoader: _phonePackageLoader(),
      ),
    );
    for (var attempt = 0; attempt < 20; attempt += 1) {
      await tester.pump(const Duration(milliseconds: 100));
      if (find.byType(TabBar).evaluate().isNotEmpty) {
        break;
      }
    }

    expect(tester.getSize(find.byType(Scaffold)).width, 430);
    expect(find.byType(TabBar), findsOneWidget);
    expect(find.text('Scene'), findsOneWidget);
    expect(find.text('Preview'), findsOneWidget);
    expect(find.text('Controls'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('composed-scene-preview')),
      findsOneWidget,
    );
    final tabController = DefaultTabController.of(
      tester.element(find.byType(TabBar)),
    );
    expect(tabController.index, 1);

    await tester.tap(find.byIcon(Icons.tune_outlined));
    await tester.pump();
    expect(tabController.index, 2);
    await tester.pump(const Duration(milliseconds: 500));

    expect(find.byKey(const ValueKey('opacity-slider')), findsOneWidget);
    expect(find.text('82%'), findsOneWidget);
  });
}

AgscenePackageLoader _phonePackageLoader() => AgscenePackageLoader(
  (_) async => '''
{
  "format": "facetwire.agent-scene-package",
  "version": "0.1",
  "id": "document:phone-layout",
  "title": "Phone Layout",
  "canvas": {
    "id": "canvas:phone-layout",
    "size": {"width": 650, "height": 366},
    "pages": [{
      "id": "page:phone-layout",
      "size": {"width": 650, "height": 366},
      "layers": [{
        "id": "layer:phone-layout",
        "z": 0,
        "zones": [{
          "id": "zone:phone-layout",
          "bounds": {"x": 0, "y": 0, "width": 650, "height": 366},
          "content": {
            "type": "placeholder",
            "kind": "image",
            "reason": "resource_unavailable",
            "mode": "standard",
            "label": "Phone layout placeholder",
            "permittedActions": ["retry"]
          }
        }]
      }]
    }]
  },
  "resources": []
}
''',
);

final class _FakeClient implements NativeDemoClient {
  var renderCalls = 0;

  @override
  Future<void> close() async {}

  @override
  Future<int> hitTest(DemoNativeRequest request, double x, double y) async => 0;

  @override
  Future<Map<String, Object?>> parameterSchema() async => {
    'type': 'object',
    'properties': <String, Object?>{},
  };

  @override
  Future<DemoRenderReport> render(DemoNativeRequest request) async {
    renderCalls += 1;
    return DemoRenderReport(
      contract: const {'validationStatus': 0, 'normalizationFlags': 0},
      measure: {'width': request.width, 'height': request.height, 'source': 1},
      render: const {
        'commandCount': 1,
        'visualDensity': 5,
        'visibleActions': 1,
      },
      semantics: {
        'role': 2,
        'label': request.label,
        'statusKey': 'placeholder.status.loading',
        'phase': request.phase.wireValue,
        'stale': request.stale ? 1 : 0,
      },
      commands: const [
        DemoCommand(
          op: 'fillRoundedRect',
          x: 0,
          y: 0,
          width: 640,
          height: 360,
          radius: 18,
          strokeWidth: 0,
          dashed: false,
          red: .9,
          green: .95,
          blue: 1,
          alpha: .5,
          value: '',
        ),
      ],
    );
  }

  @override
  Future<Map<String, Object?>> snapshot() async => const {
    'pluginVersion': '0.1.0',
    'state': 'ready',
  };
}
