# RealityKit Scale
A CLI photogramatry tool that models the coral garden using Apple's RealityKit.

Requires macOS 15 Sequoia or later

## Build
Requires Xcode command line tools

Build on macOS with:
```
swift build -c release
```

Run with 
```
.build/release/realitykit_scale --input [INPUT_DIR] --detail [preview, reduced, medium, full or raw] --output [OUTPUT_PATH.usdz] --checkpoint ./checkpoints --sequential --high-sensitivity
```