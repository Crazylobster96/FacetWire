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

    func testNativeFlowLayoutReturnsThreeBalancedFragments() throws {
        let report = try FacetWireBridge.composeFlow(contentCase: 0)

        XCTAssertTrue(report.nativeRuntime)
        XCTAssertEqual(report.capability, "facetwire.layout.flow")
        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertTrue(report.complete)
        XCTAssertTrue(report.pagesBalanced)
        XCTAssertFalse(report.inlineObjects)
        XCTAssertEqual(report.fragmentCount, 3)
        XCTAssertEqual(report.fragments.map(\.kind), ["text", "object", "text"])
    }

    func testNativeFlowLayoutPreservesUnknownObjectAsPlaceholder() throws {
        let report = try FacetWireBridge.composeFlow(contentCase: 2)

        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertEqual(report.fragments[1].kind, "placeholder")
        XCTAssertEqual(report.fragments[1].sourceItemId, "object.missing.level-3")
    }

    func testVirtualPagesProducesThreeBalancedPages() throws {
        let report = try FacetWireBridge.composeFlow(
            contentCase: 2,
            pageMode: .virtualPages
        )

        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertTrue(report.complete)
        XCTAssertTrue(report.pagesBalanced)
        XCTAssertEqual(report.pageCount, 2)
        XCTAssertEqual(report.fragmentCount, 3)
        XCTAssertEqual(report.fragments.map(\.pageIndex), [0, 0, 1])
        XCTAssertEqual(report.fragments[0].sourceItemId, "paragraph.intro.level-3")
        XCTAssertEqual(report.fragments[1].sourceItemId, "object.missing.level-3")
    }

    func testColumnsProducesTwoColumnPlanWithoutChangingContent() throws {
        let report = try FacetWireBridge.composeFlow(
            contentCase: 2,
            pageMode: .columns
        )

        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertTrue(report.complete)
        XCTAssertTrue(report.pagesBalanced)
        XCTAssertEqual(report.pageCount, 1)
        XCTAssertEqual(report.columnCount, 2)
        XCTAssertEqual(report.fragments.map(\.pageIndex), [0, 0, 0])
        XCTAssertEqual(report.fragments.map(\.columnIndex), [0, 0, 1])
        XCTAssertEqual(report.fragments[1].kind, "placeholder")
        XCTAssertEqual(report.fragments[1].sourceItemId, "object.missing.level-3")
    }

    func testInlineObjectIsAtomicAndPreservesTextRanges() throws {
        let report = try FacetWireBridge.composeFlow(contentCase: 3)

        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertTrue(report.complete)
        XCTAssertTrue(report.inlineObjects)
        XCTAssertEqual(report.fragmentCount, 3)
        XCTAssertEqual(report.fragments.map(\.kind), ["text", "object", "text"])
        XCTAssertEqual(report.fragments[0].sourceItemId, "paragraph.inline.level-1")
        XCTAssertEqual(report.fragments[1].sourceItemId, "image.inline.level-1")
        XCTAssertEqual(report.fragments[0].textStart, 0)
        XCTAssertEqual(report.fragments[0].textEnd, 7)
        XCTAssertEqual(report.fragments[2].textStart, 7)
        XCTAssertEqual(report.fragments[2].textEnd, 21)
    }

    func testInlineFallbackWorksAcrossColumns() throws {
        let report = try FacetWireBridge.composeFlow(
            contentCase: 5,
            pageMode: .columns
        )

        XCTAssertEqual(report.composeStatus, 0)
        XCTAssertTrue(report.complete)
        XCTAssertTrue(report.inlineObjects)
        XCTAssertEqual(report.pageCount, 1)
        XCTAssertEqual(report.columnCount, 2)
        XCTAssertEqual(report.fragmentCount, 3)
        XCTAssertEqual(report.fragments[1].kind, "placeholder")
        XCTAssertEqual(
            report.fragments[1].sourceItemId,
            "object.inline-missing.level-3"
        )
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
