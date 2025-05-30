// swift-tools-version:5.3
import PackageDescription

let package = Package(
    name: "TreeSitterink",
    products: [
        .library(name: "TreeSitterink", targets: ["TreeSitterink"]),
    ],
    dependencies: [
        .package(url: "https://github.com/ChimeHQ/SwiftTreeSitter", from: "0.8.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterink",
            dependencies: [],
            path: ".",
            sources: [
                "src/parser.c",
                // NOTE: if your language has an external scanner, add it here.
            ],
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterinkTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterink",
            ],
            path: "bindings/swift/TreeSitterinkTests"
        )
    ],
    cLanguageStandard: .c11
)
