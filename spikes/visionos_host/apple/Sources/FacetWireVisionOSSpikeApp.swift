// SPDX-License-Identifier: MPL-2.0
import SwiftUI

@main
struct FacetWireVisionOSSpikeApp: App {
    var body: some Scene {
        WindowGroup {
            SpatialSurfaceScreen(canOpenVolume: true)
        }
        .defaultSize(width: 980, height: 720)

        WindowGroup(id: "facetwire-volume") {
            SpatialSurfaceScreen(canOpenVolume: false)
        }
        .windowStyle(.volumetric)
        .defaultSize(width: 1.0, height: 0.625, depth: 0.08, in: .meters)
    }
}
