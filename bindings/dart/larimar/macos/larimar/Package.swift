// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "larimar",
    platforms: [.macOS("12.0")],
    products: [
        .library(name: "larimar", targets: ["larimar"]),
    ],
    dependencies: [
        .package(name: "FlutterFramework", path: "../FlutterFramework"),
    ],
    targets: [
        .target(
            name: "larimar",
            dependencies: [
                .product(name: "FlutterFramework", package: "FlutterFramework"),
            ],
            path: "Sources/larimar",
            publicHeadersPath: "include",
            linkerSettings: [
                .linkedFramework("CoreVideo"),
                .linkedFramework("Metal"),
                .linkedFramework("IOSurface"),
                .linkedFramework("CoreGraphics"),
                .linkedFramework("ImageIO"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx11
)
