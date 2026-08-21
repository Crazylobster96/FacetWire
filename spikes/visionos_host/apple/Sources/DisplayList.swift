// SPDX-License-Identifier: MPL-2.0
import Foundation
import SwiftUI

enum DisplayOpcode: UInt8 {
    case fillRect = 1
    case fillRoundedRect = 2
    case strokeRoundedRect = 3
}

struct DisplayCommand: Equatable {
    let opcode: DisplayOpcode
    let rect: CGRect
    let radius: CGFloat
    let red: Double
    let green: Double
    let blue: Double
    let alpha: Double

    var color: Color {
        Color(red: red, green: green, blue: blue, opacity: alpha)
    }
}

enum DisplayListError: Error, Equatable {
    case shortHeader
    case invalidMagic
    case unsupportedHeader(version: UInt16, size: UInt16)
    case invalidLength
    case invalidOpcode(UInt8)
    case invalidValue
}

enum DisplayListDecoder {
    static let headerSize = 12
    static let commandSize = 40
    static let maximumCommandCount = 100_000

    static func decode(_ data: Data) throws -> [DisplayCommand] {
        guard data.count >= headerSize else { throw DisplayListError.shortHeader }
        guard Array(data[0..<4]) == [0x46, 0x57, 0x44, 0x4c] else {
            throw DisplayListError.invalidMagic
        }
        let version = readUInt16(data, 4)
        let encodedHeaderSize = readUInt16(data, 6)
        guard version == 1, encodedHeaderSize == headerSize else {
            throw DisplayListError.unsupportedHeader(
                version: version,
                size: encodedHeaderSize
            )
        }
        let commandCount = Int(readUInt32(data, 8))
        guard commandCount <= maximumCommandCount,
              data.count == headerSize + commandCount * commandSize else {
            throw DisplayListError.invalidLength
        }

        return try (0..<commandCount).map { index in
            let offset = headerSize + index * commandSize
            guard data[offset + 1] == 0, data[offset + 2] == 0,
                  data[offset + 3] == 0 else {
                throw DisplayListError.invalidValue
            }
            guard let opcode = DisplayOpcode(rawValue: data[offset]) else {
                throw DisplayListError.invalidOpcode(data[offset])
            }
            let values = (0..<9).map { valueIndex in
                Double(readFloat(data, offset + 4 + valueIndex * 4))
            }
            guard values.allSatisfy(\.isFinite),
                  values[2] >= 0, values[3] >= 0, values[4] >= 0,
                  values[5...8].allSatisfy({ 0...1 ~= $0 }) else {
                throw DisplayListError.invalidValue
            }
            return DisplayCommand(
                opcode: opcode,
                rect: CGRect(x: values[0], y: values[1],
                             width: values[2], height: values[3]),
                radius: values[4],
                red: values[5], green: values[6], blue: values[7],
                alpha: values[8]
            )
        }
    }

    private static func readUInt16(_ data: Data, _ offset: Int) -> UInt16 {
        UInt16(data[offset]) | (UInt16(data[offset + 1]) << 8)
    }

    private static func readUInt32(_ data: Data, _ offset: Int) -> UInt32 {
        UInt32(data[offset]) |
            (UInt32(data[offset + 1]) << 8) |
            (UInt32(data[offset + 2]) << 16) |
            (UInt32(data[offset + 3]) << 24)
    }

    private static func readFloat(_ data: Data, _ offset: Int) -> Float {
        Float(bitPattern: readUInt32(data, offset))
    }
}
