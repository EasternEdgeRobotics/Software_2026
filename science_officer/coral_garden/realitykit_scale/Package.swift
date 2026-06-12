// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "RealityKitScake",
    platforms: [
        .macOS(.v26)
    ],
    products: [
        .executable(
            name: "realitykit_scale",
            targets: ["realitykit_scale"]
        )
    ],
    targets: [
        .executableTarget(
            name: "realitykit_scale"
        )
    ]
)