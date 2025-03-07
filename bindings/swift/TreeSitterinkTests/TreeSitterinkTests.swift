import XCTest
import SwiftTreeSitter
import TreeSitterink

final class TreeSitterinkTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_ink())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading ink grammar")
    }
}
