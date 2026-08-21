// SPDX-License-Identifier: MPL-2.0
import Foundation
import XCTest
@testable import FacetWireVisionOSSpike

final class DisplayListTests: XCTestCase {
    func testDecodesOneCommand() throws {
        var bytes = Data(repeating: 0, count: 52)
        bytes.replaceSubrange(0..<4, with: [0x46, 0x57, 0x44, 0x4c])
        writeUInt16(1, to: &bytes, at: 4)
        writeUInt16(12, to: &bytes, at: 6)
        writeUInt32(1, to: &bytes, at: 8)
        bytes[12] = 1
        let values: [Float] = [0, 0, 100, 50, 0, 0.1, 0.2, 0.3, 0.4]
        for (index, value) in values.enumerated() {
            writeUInt32(value.bitPattern, to: &bytes, at: 16 + index * 4)
        }

        let commands = try DisplayListDecoder.decode(bytes)

        XCTAssertEqual(commands.count, 1)
        XCTAssertEqual(commands[0].rect.width, 100)
        XCTAssertEqual(commands[0].alpha, 0.4, accuracy: 0.0001)
    }

    func testRejectsUnknownVersion() {
        var bytes = Data(repeating: 0, count: 12)
        bytes.replaceSubrange(0..<4, with: [0x46, 0x57, 0x44, 0x4c])
        writeUInt16(2, to: &bytes, at: 4)
        writeUInt16(12, to: &bytes, at: 6)
        XCTAssertThrowsError(try DisplayListDecoder.decode(bytes))
    }

    private func writeUInt16(_ value: UInt16, to data: inout Data, at offset: Int) {
        data[offset] = UInt8(value & 0xff)
        data[offset + 1] = UInt8((value >> 8) & 0xff)
    }

    private func writeUInt32(_ value: UInt32, to data: inout Data, at offset: Int) {
        for index in 0..<4 {
            data[offset + index] = UInt8((value >> UInt32(index * 8)) & 0xff)
        }
    }
}
