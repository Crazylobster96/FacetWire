// SPDX-License-Identifier: MPL-2.0
import 'package:flutter/material.dart';

import 'src/demo_app.dart';
import 'src/demo_models.dart';
import 'src/native_demo_client.dart';
import 'src/package_loader.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  NativeDemoClient client;
  try {
    client = FfiNativeDemoClient.open();
  } on Object catch (error) {
    client = UnavailableNativeDemoClient(error.toString());
  }
  runApp(
    PlaceholderDemoApp(
      client: client,
      packageLoader: AgscenePackageLoader.asset(),
    ),
  );
}
