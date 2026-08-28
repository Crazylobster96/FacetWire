// SPDX-License-Identifier: MPL-2.0
import 'package:flutter/material.dart';
import 'package:media_kit/media_kit.dart';

import 'src/models.dart';
import 'src/native_runtime_client.dart';
import 'src/spike_app.dart';

void main(List<String> arguments) {
  WidgetsFlutterBinding.ensureInitialized();
  MediaKit.ensureInitialized();
  NativeRuntimeClient client;
  try {
    client = NativeAssetRuntimeClient.open();
  } on Object {
    // Widget tests and unsupported development hosts retain a deterministic
    // fallback; platform verification must show nativeRuntime=true in Flow UI.
    client = DemoRuntimeClient();
  }
  runApp(
    SpikeApp(
      client: client,
      initialDemoPath: parseDemoPathArguments(arguments),
    ),
  );
}
