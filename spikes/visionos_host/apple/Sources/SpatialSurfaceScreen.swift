// SPDX-License-Identifier: MPL-2.0
import SwiftUI

struct SpatialSurfaceScreen: View {
    let canOpenVolume: Bool

    @Environment(\.openWindow) private var openWindow
    @State private var opacity = 0.75
    @State private var frame: FacetWireFrame?
    @State private var diagnostic = "Loading FacetWire C bridge..."

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text("FacetWire visionOS Host Spike")
                        .font(.title2.bold())
                    Text("SpatialSurface v0.1 · static C bridge · DisplayList v1")
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if canOpenVolume {
                    Button("Open volume") {
                        openWindow(id: "facetwire-volume")
                    }
                }
            }

            Group {
                if let frame {
                    DisplayListSurface(commands: frame.commands)
                        .accessibilityElement(children: .ignore)
                        .accessibilityLabel(frame.accessibilityLabel)
                } else {
                    ContentUnavailableView(
                        "Unable to render surface",
                        systemImage: "rectangle.slash",
                        description: Text(diagnostic)
                    )
                }
            }
            .aspectRatio(16 / 9, contentMode: .fit)
            .glassBackgroundEffect()

            HStack(spacing: 14) {
                Text("Surface opacity")
                Slider(value: $opacity, in: 0...1)
                    .accessibilityValue("\(Int(opacity * 100)) percent")
                Text("\(Int(opacity * 100))%")
                    .monospacedDigit()
                    .frame(width: 52, alignment: .trailing)
            }
            Text(diagnostic)
                .font(.footnote.monospaced())
                .foregroundStyle(.secondary)
        }
        .padding(28)
        .task { reload() }
        .onChange(of: opacity) { _, _ in reload() }
    }

    private func reload() {
        do {
            frame = try FacetWireBridge.render(
                width: 640,
                height: 360,
                opacity: Float(opacity)
            )
            diagnostic = "PASS · native ABI v1 · 3 DisplayList commands"
        } catch {
            frame = nil
            diagnostic = "FAIL · \(error)"
        }
    }
}

private struct DisplayListSurface: View {
    let commands: [DisplayCommand]

    var body: some View {
        Canvas { context, size in
            let scaleX = size.width / 640
            let scaleY = size.height / 360
            for command in commands {
                let rect = CGRect(
                    x: command.rect.minX * scaleX,
                    y: command.rect.minY * scaleY,
                    width: command.rect.width * scaleX,
                    height: command.rect.height * scaleY
                )
                let path = Path(
                    roundedRect: rect,
                    cornerRadius: command.radius * min(scaleX, scaleY)
                )
                switch command.opcode {
                case .fillRect, .fillRoundedRect:
                    context.fill(path, with: .color(command.color))
                case .strokeRoundedRect:
                    context.stroke(path, with: .color(command.color), lineWidth: 2)
                }
            }
        }
    }
}
