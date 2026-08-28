// SPDX-License-Identifier: MPL-2.0
import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (input.config.buildCodeAssets) {
      final builder = CBuilder.library(
        name: 'facetwire_ui_spike',
        assetName: 'facetwire_ui_spike.dart',
        sources: const [
          'native/src/facetwire_ui_spike.c',
          '../../plugins/flow_layout/src/plugin.c',
        ],
        includes: const ['native/include', '../../include'],
        defines: const {'FWUI_BUILDING_LIBRARY': '1'},
      );
      await builder.run(input: input, output: output);
    }
  });
}
