// SPDX-License-Identifier: MPL-2.0
import 'dart:io';

import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets(
    'DOCX images share cache keys and decode within budget with contain',
    (tester) async {
      final bytes = (await tester.runAsync(
        () => File(
          'assets/documents/rich-media-showcase.agscene/resources/showcase-image.png',
        ).readAsBytes(),
      ))!;
      final asset = DocumentImageAsset(bytes, maxDecodePixels: 40000);
      final first = DocumentImage.fromAsset(
        const DocumentSource('h', 'p', '/a'),
        asset,
        width: 2000,
        height: 2000,
      );
      final second = DocumentImage.fromAsset(
        const DocumentSource('h', 'p', '/b'),
        asset,
        width: 2000,
        height: 2000,
      );
      await tester.pumpWidget(
        MaterialApp(
          home: MediaQuery(
            data: const MediaQueryData(devicePixelRatio: 3),
            child: Row(
              children: [
                for (final block in [first, second])
                  SizedBox(
                    width: 200,
                    height: 200,
                    child: DocumentBlockView(
                      item: PreparedDocumentBlock(block, 2000, 2000),
                    ),
                  ),
              ],
            ),
          ),
        ),
      );
      final widgets = tester.widgetList<Image>(find.byType(Image)).toList();
      final provider = widgets.first.image as ResizeImage;
      expect(provider.width! * provider.height!, lessThanOrEqualTo(40000));
      expect(provider.policy, ResizeImagePolicy.fit);
      expect(provider.allowUpscaling, isFalse);
      expect(widgets.first.fit, BoxFit.contain);
      expect(
        await provider.obtainKey(ImageConfiguration.empty),
        await widgets.last.image.obtainKey(ImageConfiguration.empty),
      );
      await tester.runAsync(
        () => precacheImage(provider, tester.element(find.byType(Row))),
      );
      await tester.pumpAndSettle();
      final rasters = tester
          .widgetList<RawImage>(find.byType(RawImage))
          .toList();
      expect(rasters, hasLength(2));
      for (final raster in rasters) {
        expect(raster.image, isNotNull);
        expect(
          raster.image!.width * raster.image!.height,
          lessThanOrEqualTo(40000),
        );
        // The source is landscape; a square decode request must not distort it.
        expect(raster.image!.width, greaterThan(raster.image!.height));
      }
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'decode target respects display size, DPR and thin aspect ratios',
    (tester) async {
      final bytes = (await tester.runAsync(
        () => File(
          'assets/documents/rich-media-showcase.agscene/resources/showcase-image.png',
        ).readAsBytes(),
      ))!;
      for (final size in [
        const Size(60, 40),
        const Size(200000000, 1),
        const Size(1, 200000000),
      ]) {
        final block = DocumentImage.fromAsset(
          const DocumentSource('h', 'p', '/a'),
          DocumentImageAsset(bytes),
          width: size.width,
          height: size.height,
        );
        await tester.pumpWidget(
          MaterialApp(
            home: MediaQuery(
              data: const MediaQueryData(devicePixelRatio: 2),
              child: DocumentBlockView(
                item: PreparedDocumentBlock(block, size.width, size.height),
                displayScale: 1.5,
              ),
            ),
          ),
        );
        final provider =
            tester.widget<Image>(find.byType(Image)).image as ResizeImage;
        expect(provider.width! * provider.height!, lessThanOrEqualTo(1000000));
        expect(provider.width, greaterThan(0));
        expect(provider.height, greaterThan(0));
        if (size.width == 60) {
          expect(provider.width, 180);
          expect(provider.height, 120);
        }
      }
      await tester.pumpWidget(const SizedBox());
      expect(tester.takeException(), isNull);
    },
  );
}
