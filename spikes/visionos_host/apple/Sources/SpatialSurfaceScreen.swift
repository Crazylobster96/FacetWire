// SPDX-License-Identifier: MPL-2.0
import SwiftUI

struct SpatialSurfaceScreen: View {
    let canOpenVolume: Bool

    @Environment(\.openWindow) private var openWindow
    @State private var opacity = 0.75
    @State private var frame: FacetWireFrame?
    @State private var placeholderDiagnostic = "Loading FacetWire C bridge..."
    @State private var selectedLevel = 0
    @State private var virtualPagesProbe = false
    @State private var flowOpacity = 0.9
    @State private var flowReport: FacetWireFlowReport?
    @State private var flowDiagnostic = "Loading Flow Layout 0.1..."

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                header
                placeholderSection
                Divider()
                flowSection
            }
            .padding(28)
        }
        .task { reloadAll() }
        .onChange(of: opacity) { _, _ in reloadPlaceholder() }
        .onChange(of: selectedLevel) { _, _ in reloadFlow() }
        .onChange(of: virtualPagesProbe) { _, _ in reloadFlow() }
    }

    private var header: some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("FacetWire visionOS Verification Host")
                    .font(.title2.bold())
                Text("Static C registration · Placeholder + Flow Layout 0.1")
                    .foregroundStyle(.secondary)
            }
            Spacer()
            if canOpenVolume {
                Button("Open volume") {
                    openWindow(id: "facetwire-volume")
                }
            }
        }
    }

    private var placeholderSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Placeholder Renderer")
                .font(.headline)
            Group {
                if let frame {
                    DisplayListSurface(commands: frame.commands)
                        .accessibilityElement(children: .ignore)
                        .accessibilityLabel(frame.accessibilityLabel)
                } else {
                    ContentUnavailableView(
                        "Unable to render surface",
                        systemImage: "rectangle.slash",
                        description: Text(placeholderDiagnostic)
                    )
                }
            }
            .aspectRatio(16 / 9, contentMode: .fit)
            .frame(maxHeight: 300)
            .glassBackgroundEffect()

            HStack(spacing: 14) {
                Text("Surface opacity")
                Slider(value: $opacity, in: 0...1)
                    .accessibilityValue("\(Int(opacity * 100)) percent")
                Text("\(Int(opacity * 100))%")
                    .monospacedDigit()
                    .frame(width: 52, alignment: .trailing)
            }
            Text(placeholderDiagnostic)
                .font(.footnote.monospaced())
                .foregroundStyle(.secondary)
        }
    }

    private var flowSection: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack {
                VStack(alignment: .leading) {
                    Text("Flow Layout 0.1")
                        .font(.headline)
                    Text("Three recursive cases · continuous / virtual-pages + block")
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Picker("Scene level", selection: $selectedLevel) {
                    Text("Level 1").tag(0)
                    Text("Level 2").tag(1)
                    Text("Level 3").tag(2)
                }
                .pickerStyle(.segmented)
                .frame(maxWidth: 360)
            }

            Toggle("Use virtual-pages", isOn: $virtualPagesProbe)

            Group {
                if let report = flowReport, report.complete {
                    FlowLayoutSurface(report: report)
                        .opacity(flowOpacity)
                        .accessibilityLabel(
                            "Flow Layout \(selectedLevel + 1), \(report.fragmentCount) fragments"
                        )
                } else if let report = flowReport {
                    ContentUnavailableView(
                        "Unsupported layout slice",
                        systemImage: "doc.badge.ellipsis",
                        description: Text("composeStatus \(report.composeStatus)")
                    )
                } else {
                    ContentUnavailableView(
                        "Unable to compose Flow",
                        systemImage: "square.stack.3d.up.slash",
                        description: Text(flowDiagnostic)
                    )
                }
            }
            .aspectRatio(
                CGFloat(
                    (flowReport?.continuousExtent.width ?? 600) /
                        (flowReport?.continuousExtent.height ?? 700)
                ),
                contentMode: .fit
            )
            .frame(maxHeight: 520)
            .glassBackgroundEffect()

            HStack(spacing: 14) {
                Text("Viewer opacity")
                Slider(value: $flowOpacity, in: 0...1)
                    .accessibilityValue("\(Int(flowOpacity * 100)) percent")
                Text("\(Int(flowOpacity * 100))%")
                    .monospacedDigit()
                    .frame(width: 52, alignment: .trailing)
            }
            Text(flowDiagnostic)
                .font(.footnote.monospaced())
                .foregroundStyle(flowReport?.nativeRuntime == true ? .green : .secondary)
            if let report = flowReport {
                Text("\(report.capability) · \(report.supportedSlice) · \(report.planKey)")
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func reloadAll() {
        reloadPlaceholder()
        reloadFlow()
    }

    private func reloadPlaceholder() {
        do {
            frame = try FacetWireBridge.render(
                width: 640,
                height: 360,
                opacity: Float(opacity)
            )
            placeholderDiagnostic = "PASS · native ABI v1 · 3 DisplayList commands"
        } catch {
            frame = nil
            placeholderDiagnostic = "FAIL · \(error)"
        }
    }

    private func reloadFlow() {
        do {
            let report = try FacetWireBridge.composeFlow(
                contentCase: UInt32(selectedLevel),
                virtualPages: virtualPagesProbe
            )
            flowReport = report
            if virtualPagesProbe {
                flowDiagnostic = report.nativeRuntime && report.complete &&
                    report.pageCount == 3 && report.pagesBalanced
                    ? "PASS · native virtual-pages · 3 balanced pages"
                    : "FAIL · native virtual-pages contract incomplete"
            } else {
                flowDiagnostic = report.nativeRuntime && report.complete
                    ? "PASS · native Flow · Level \(selectedLevel + 1) · \(report.fragmentCount) fragments"
                    : "FAIL · native Flow contract incomplete"
            }
        } catch {
            flowReport = nil
            flowDiagnostic = "FAIL · \(error)"
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

private struct FlowLayoutSurface: View {
    let report: FacetWireFlowReport

    var body: some View {
        Canvas { context, size in
            let logicalWidth = CGFloat(report.continuousExtent.width)
            let logicalHeight = CGFloat(report.continuousExtent.height)
            let scale = min(size.width / logicalWidth, size.height / logicalHeight)
            let offsetX = (size.width - logicalWidth * scale) / 2
            let offsetY = (size.height - logicalHeight * scale) / 2
            for pageIndex in 0..<report.pageCount {
                let pageTop = CGFloat(pageIndex) *
                    CGFloat(report.pageSize.height + report.pageGap)
                let page = CGRect(
                    x: offsetX,
                    y: offsetY + pageTop * scale,
                    width: CGFloat(report.pageSize.width) * scale,
                    height: CGFloat(report.pageSize.height) * scale
                )
                var pagePath = Path()
                pagePath.addRect(page)
                context.fill(pagePath, with: .color(.white.opacity(0.82)))
                context.stroke(pagePath, with: .color(.indigo), lineWidth: 2)
            }

            for fragment in report.fragments {
                let bounds = fragment.bounds
                let pageTop = CGFloat(fragment.pageIndex) *
                    CGFloat(report.pageSize.height + report.pageGap)
                let rect = CGRect(
                    x: offsetX + CGFloat(bounds.x) * scale,
                    y: offsetY + (pageTop + CGFloat(bounds.y)) * scale,
                    width: CGFloat(bounds.width) * scale,
                    height: CGFloat(bounds.height) * scale
                )
                let color: Color
                switch fragment.kind {
                case "text":
                    color = .indigo.opacity(0.26)
                case "placeholder":
                    color = .orange.opacity(0.48)
                default:
                    color = .blue.opacity(0.44)
                }
                let path = Path(roundedRect: rect, cornerRadius: 8 * scale)
                context.fill(path, with: .color(color))
                context.stroke(path, with: .color(color.opacity(1)), lineWidth: 2)
            }
        }
    }
}
