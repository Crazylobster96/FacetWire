// SPDX-License-Identifier: MPL-2.0
import 'package:flutter/material.dart';
import 'package:media_kit/media_kit.dart';

import 'src/demo_app.dart';
import 'src/chart_runtime_client.dart';
import 'src/demo_models.dart';
import 'src/flow_runtime_client.dart';
import 'src/native_asset_demo_client.dart';
import 'src/native_demo_client.dart';
import 'src/package_loader.dart';

Future<void> main(List<String> arguments) async {
  WidgetsFlutterBinding.ensureInitialized();
  MediaKit.ensureInitialized();

  NativeDemoClient placeholderClient;
  try {
    placeholderClient = NativeAssetDemoClient.open();
  } on Object catch (assetError) {
    try {
      placeholderClient = FfiNativeDemoClient.open();
    } on Object catch (ffiError) {
      placeholderClient = UnavailableNativeDemoClient(
        'Native Assets: $assetError\nDynamic library: $ffiError',
      );
    }
  }

  NativeRuntimeClient flowClient;
  try {
    flowClient = NativeAssetRuntimeClient.open();
  } on Object {
    flowClient = const DemoRuntimeClient();
  }

  ChartRuntimeClient chartClient;
  try {
    chartClient = NativeAssetChartRuntimeClient.open();
  } on Object {
    chartClient = const DemoChartRuntimeClient();
  }

  runApp(
    PlaceholderDemoApp(
      client: placeholderClient,
      runtimeClient: flowClient,
      chartClient: chartClient,
      packageLoader: AgscenePackageLoader.asset(),
      initialDemoPath: parseDemoPathArguments(arguments),
    ),
  );
}
