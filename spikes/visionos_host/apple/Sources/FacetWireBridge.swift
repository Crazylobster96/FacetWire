// SPDX-License-Identifier: MPL-2.0
import Foundation

struct FacetWireFrame {
    let commands: [DisplayCommand]
    let accessibilityLabel: String
}

enum FacetWireBridgeError: Error {
    case create(Int32)
    case render(Int32)
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
}
