// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:facetwire_placeholder_demo/src/demo_models.dart';
import 'package:facetwire_placeholder_demo/src/native_demo_client.dart';

Future<void> main(List<String> arguments) async {
  if (arguments.length != 1) {
    stderr.writeln('Usage: dart run tool/native_smoke.dart <bridge-library>');
    exitCode = 64;
    return;
  }
  final client = FfiNativeDemoClient.open(libraryPath: arguments.single);
  try {
    final snapshot = await client.snapshot();
    if (snapshot['state'] != 'ready' ||
        snapshot['capabilityId'] != 'facetwire.renderer.placeholder') {
      throw StateError('Unexpected runtime snapshot: $snapshot');
    }
    const request = DemoNativeRequest(
      width: 640,
      height: 360,
      opacity: .5,
      backgroundAlpha: .8,
      fontScale: 1,
      devicePixelRatio: 1,
      reason: PlaceholderReason.rendererMissing,
      mode: PlaceholderMode.standard,
      permittedActions: PlaceholderAction.all,
      phase: PlaceholderPhase.readyForHandoff,
      progressKind: 2,
      completed: 3,
      total: 4,
      stale: false,
      prefersDark: false,
      highContrast: false,
      reduceMotion: false,
      measureCase: MeasureCase.resolved,
      presentationRevision: 1,
      contentKind: 'chart',
      label: 'Native FFI smoke chart',
    );
    final report = await client.render(request);
    if (report.contract['validationStatus'] != 0 ||
        report.commands.isEmpty ||
        report.semantics['role'] != 5) {
      throw StateError('Unexpected render report.');
    }
    final action = await client.hitTest(request, 20, 330);
    if (action == 0) {
      throw StateError('Expected native action hit.');
    }
    stdout.writeln(
      'FacetWire Dart FFI smoke passed: '
      '${report.commands.length} commands, action=$action.',
    );
  } finally {
    await client.close();
  }
}
