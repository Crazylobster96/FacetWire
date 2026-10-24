// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:facetwire_placeholder_demo/src/core_content_demo.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  for (final show in <bool?>[null, true, false]) {
    testWidgets(
      'recursive diagnostic badges: $show',
      (tester) async {
        final semantics = tester.ensureSemantics();
        await tester.binding.setSurfaceSize(const Size(1600, 1200));
        addTearDown(() => tester.binding.setSurfaceSize(null));
        final path = Directory(
          'assets/documents/core-content-overlap-demo.agscene',
        ).absolute.path;
        final screen = show == null
            ? CoreContentDemoScreen(initialPath: path)
            : CoreContentDemoScreen(
                initialPath: path,
                showDiagnosticBadges: show,
              );
        try {
          await tester.pumpWidget(MaterialApp(home: screen));
          for (
            var i = 0;
            i < 100 &&
                find.textContaining('3 recursive documents').evaluate().isEmpty;
            i++
          ) {
            await tester.runAsync(
              () => Future<void>.delayed(const Duration(milliseconds: 20)),
            );
            await tester.pump(const Duration(milliseconds: 50));
          }
          expect(find.textContaining('3 recursive documents'), findsOneWidget);
          final visible = show ?? true;
          expect(
            find.textContaining(
              RegExp(
                r'^(text|image|animated-image|document) · [0-9]+\.[0-9]{2}$',
              ),
            ),
            visible ? findsWidgets : findsNothing,
          );
          expect(
            find.textContaining(
              RegExp(r'^[123] · document:core-content-demo:level-[123]$'),
            ),
            visible ? findsNWidgets(3) : findsNothing,
          );
          expect(find.byType(Image), findsWidgets);
          expect(
            find.bySemanticsLabel('Level 1 blue geometry'),
            findsOneWidget,
          );
          await tester.tap(find.byType(Image).first);
          await tester.pump(const Duration(milliseconds: 50));
          expect(find.text('type: image'), findsOneWidget);
        } finally {
          await tester.pumpWidget(const SizedBox.shrink());
          semantics.dispose();
        }
      },
      variant: TargetPlatformVariant({
        TargetPlatform.iOS,
        TargetPlatform.macOS,
      }),
    );
  }
}
