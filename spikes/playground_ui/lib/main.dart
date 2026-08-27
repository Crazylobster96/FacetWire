// SPDX-License-Identifier: MPL-2.0
import 'package:flutter/material.dart';
import 'package:media_kit/media_kit.dart';

import 'src/spike_app.dart';

void main(List<String> arguments) {
  WidgetsFlutterBinding.ensureInitialized();
  MediaKit.ensureInitialized();
  runApp(
    SpikeApp(
      client: DemoRuntimeClient(),
      initialDemoPath: parseDemoPathArguments(arguments),
    ),
  );
}
