// SPDX-License-Identifier: MPL-2.0
import Foundation

struct FacetWireFrame {
    let commands: [DisplayCommand]
    let accessibilityLabel: String
}

struct FacetWireFlowBounds: Decodable {
    let x: Double
    let y: Double
    let width: Double
    let height: Double
}

struct FacetWireFlowFragment: Decodable, Identifiable {
    let kind: String
    let sourceItemId: String
    let contentKind: String
    let bounds: FacetWireFlowBounds

    var id: String { sourceItemId }
}

struct FacetWireFlowReport: Decodable {
    let pluginId: String
    let capability: String
    let composeStatus: Int
    let complete: Bool
    let pageCount: Int
    let fragmentCount: Int
    let planKey: String
    let pagesBalanced: Bool
    let supportedSlice: String
    let nativeRuntime: Bool
    let fragments: [FacetWireFlowFragment]
}

enum FacetWireBridgeError: Error {
    case create(Int32)
    case render(Int32)
    case compose(Int32)
    case missingBuffer
    case invalidSemantics
}

enum FacetWireBridge {
    static func render(width: Float, height: Float, opacity: Float) throws -> FacetWireFrame {
        var context: OpaquePointer?
        let createStatus = fwui_context_create(&context)
        guard createStatus == FWUI_STATUS_OK else {
            throw FacetWireBridgeError.create(Int32(createStatus.rawValue))
        }
        defer { fwui_context_destroy(context) }

        var display = fwui_buffer(data: nil, length: 0)
        var semantics = fwui_buffer(data: nil, length: 0)
        defer {
            fwui_buffer_release(&display)
            fwui_buffer_release(&semantics)
        }
        let renderStatus = fwui_render_placeholder(
            context, width, height, opacity, &display, &semantics
        )
        guard renderStatus == FWUI_STATUS_OK else {
            throw FacetWireBridgeError.render(Int32(renderStatus.rawValue))
        }
        guard let displayPointer = display.data,
              let semanticsPointer = semantics.data else {
            throw FacetWireBridgeError.missingBuffer
        }

        let displayData = Data(bytes: displayPointer, count: Int(display.length))
        let semanticsData = Data(bytes: semanticsPointer, count: Int(semantics.length))
        let json = try JSONSerialization.jsonObject(with: semanticsData)
        guard let root = json as? [String: Any],
              let nodes = root["nodes"] as? [[String: Any]],
              let label = nodes.first?["label"] as? String else {
            throw FacetWireBridgeError.invalidSemantics
        }
        return FacetWireFrame(
            commands: try DisplayListDecoder.decode(displayData),
            accessibilityLabel: label
        )
    }

    static func composeFlow(
        width: Float = 600,
        height: Float = 700,
        demoCase: UInt32
    ) throws -> FacetWireFlowReport {
        var context: OpaquePointer?
        let createStatus = fwui_context_create(&context)
        guard createStatus == FWUI_STATUS_OK else {
            throw FacetWireBridgeError.create(Int32(createStatus.rawValue))
        }
        defer { fwui_context_destroy(context) }

        var output = fwui_buffer(data: nil, length: 0)
        defer { fwui_buffer_release(&output) }
        let composeStatus = fwui_compose_flow_demo(
            context, width, height, demoCase, &output
        )
        guard composeStatus == FWUI_STATUS_OK else {
            throw FacetWireBridgeError.compose(Int32(composeStatus.rawValue))
        }
        guard let pointer = output.data else {
            throw FacetWireBridgeError.missingBuffer
        }
        let data = Data(bytes: pointer, count: Int(output.length))
        return try JSONDecoder().decode(FacetWireFlowReport.self, from: data)
    }
}