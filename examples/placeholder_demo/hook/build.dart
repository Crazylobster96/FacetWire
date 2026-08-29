// SPDX-License-Identifier: MPL-2.0
import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final builder = CBuilder.library(
      name: 'facetwire_placeholder_demo_bridge',
      assetName: 'facetwire_placeholder_demo_bridge.dart',
      sources: const [
        'native/src/facetwire_placeholder_demo.c',
        'native/src/facetwire_playground_bridge.c',
        'native/src/facetwire_placeholder_plugin_embed.c',
        'native/src/facetwire_flow_layout_plugin_embed.c',
        'native/src/facetwire_flow_virtual_pages_embed.c',
      ],
      includes: const ['native/include', '../../include'],
      defines: const {
        'FACETWIRE_PLACEHOLDER_DEMO_BUILD': '1',
        'FWUI_BUILDING_LIBRARY': '1',
      },
    );
    await builder.run(input: input, output: output);
  });
}
