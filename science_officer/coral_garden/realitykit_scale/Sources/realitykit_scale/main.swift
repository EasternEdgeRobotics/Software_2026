import Foundation
import ImageIO
import RealityKit
import UniformTypeIdentifiers

struct CLIOptions {
    let inputDirectory: URL
    let outputURL: URL
    let detail: PhotogrammetrySession.Request.Detail
    let checkpointDirectory: URL?
    let sampleOrdering: PhotogrammetrySession.Configuration.SampleOrdering
    let featureSensitivity: PhotogrammetrySession.Configuration.FeatureSensitivity
    let convertTIFFs: Bool
    let stagingDirectory: URL?
    let keepStagingDirectory: Bool
}

enum CLIError: Error, CustomStringConvertible {
    case missingValue(String)
    case unknownArgument(String)
    case invalidDetail(String)
    case invalidInputDirectory(String)
    case noTiffImages(String)
    case invalidOutputPath(String)
    case imageConversionFailed(String)
    case objectCaptureUnsupported

    var description: String {
        switch self {
        case .missingValue(let argument):
            return "Missing value for \(argument)."

        case .unknownArgument(let argument):
            return "Unknown argument: \(argument)."

        case .invalidDetail(let detail):
            return """
            Invalid detail level: \(detail).
            Valid values: preview, reduced, medium, full, raw
            """

        case .invalidInputDirectory(let path):
            return "Input path is not a directory: \(path)"

        case .noTiffImages(let path):
            return "No .tif or .tiff images found in: \(path)"

        case .invalidOutputPath(let path):
            return "Invalid output path: \(path)"

        case .objectCaptureUnsupported:
            return """
            RealityKit Object Capture is not supported on this Mac.
            Try running on a supported macOS device with Object Capture support.
            """
        
        case .imageConversionFailed(let message):
            return "Image conversion failed: \(message)"
        }
    }
}

@main
struct PhotoModelCLI {
    static func main() async {
        let startTime = Date()

        do {
            let options = try parseArguments()

            try validate(options: options)

            guard PhotogrammetrySession.isSupported else {
                throw CLIError.objectCaptureUnsupported
            }

            try await buildModel(options: options)

            let elapsed = Date().timeIntervalSince(startTime)
            print("")
            print("Total time: \(formatDuration(elapsed))")
        } catch {
            let elapsed = Date().timeIntervalSince(startTime)

            fputs("Error: \(error)\n\n", stderr)
            fputs("Elapsed time before failure: \(formatDuration(elapsed))\n\n", stderr)

            printUsage()
            Foundation.exit(1)
        }
    }

    private static func buildModel(options: CLIOptions) async throws {
        var configuration = PhotogrammetrySession.Configuration()
        configuration.sampleOrdering = options.sampleOrdering
        configuration.featureSensitivity = options.featureSensitivity

        if let checkpointDirectory = options.checkpointDirectory {
            try FileManager.default.createDirectory(
                at: checkpointDirectory,
                withIntermediateDirectories: true
            )

            configuration.checkpointDirectory = checkpointDirectory
        }
        
        let preparedInputDirectory = try prepareInputDirectory(options: options)

        print("Input: \(options.inputDirectory.path)")

        if preparedInputDirectory != options.inputDirectory {
            print("Prepared input: \(preparedInputDirectory.path)")
        }

        print("Output: \(options.outputURL.path)")
        print("Detail: \(options.detail)")
        print("Sample ordering: \(options.sampleOrdering)")
        print("Feature sensitivity: \(options.featureSensitivity)")

        if let checkpointDirectory = options.checkpointDirectory {
            print("Checkpoint directory: \(checkpointDirectory.path)")
        }

        print("")
        print("Starting RealityKit photogrammetry session...")

        let session = try PhotogrammetrySession(
            input: preparedInputDirectory,
            configuration: configuration
        )

        let request = PhotogrammetrySession.Request.modelFile(
            url: options.outputURL,
            detail: options.detail
        )

        try session.process(requests: [request])

        for try await output in session.outputs {
            switch output {
            case .inputComplete:
                print("Input ingestion complete. Reconstruction started.")

            case .requestProgress(_, let fractionComplete):
                let percent = Int((fractionComplete * 100).rounded())
                print("Progress: \(percent)%")

            case .requestProgressInfo(_, let progressInfo):
                print("Progress info: \(progressInfo)")

            case .requestComplete(_, _):
                print("Model request complete.")

            case .requestError(_, let error):
                throw error

            case .processingComplete:
                print("")
                print("Done.")
                print("Wrote model to:")
                print(options.outputURL.path)
                return

            case .processingCancelled:
                print("Processing cancelled.")
                return

            case .invalidSample(let id, let reason):
                print("Invalid sample \(id): \(reason)")

            case .skippedSample(let id):
                print("Skipped sample \(id)")

            case .automaticDownsampling:
                print("RealityKit automatically downsampled images due to resources.")

            case .stitchingIncomplete:
                print("Stitching incomplete. RealityKit could not fully align the input images.")

            @unknown default:
                print("Received unknown RealityKit output: \(output)")
            }
        }
    }

    private static func parseArguments() throws -> CLIOptions {
        var inputDirectory: URL?
        var outputURL: URL?
        var detail: PhotogrammetrySession.Request.Detail = .medium
        var checkpointDirectory: URL?
        var sampleOrdering: PhotogrammetrySession.Configuration.SampleOrdering =
            .unordered
        var featureSensitivity: PhotogrammetrySession.Configuration.FeatureSensitivity =
            .normal

        let arguments = Array(CommandLine.arguments.dropFirst())
        var index = 0
        var convertTIFFs = true
        var stagingDirectory: URL?
        var keepStagingDirectory = false

        while index < arguments.count {
            let argument = arguments[index]

            func nextValue() throws -> String {
                let valueIndex = index + 1

                guard valueIndex < arguments.count else {
                    throw CLIError.missingValue(argument)
                }

                index += 1
                return arguments[valueIndex]
            }

            switch argument {
            case "-i", "--input":
                inputDirectory = URL(fileURLWithPath: try nextValue())
                    .standardizedFileURL

            case "-o", "--output":
                outputURL = URL(fileURLWithPath: try nextValue())
                    .standardizedFileURL

            case "-d", "--detail":
                detail = try parseDetail(try nextValue())

            case "--checkpoint":
                checkpointDirectory = URL(fileURLWithPath: try nextValue())
                    .standardizedFileURL

            case "--sequential":
                sampleOrdering = .sequential

            case "--unordered":
                sampleOrdering = .unordered

            case "--high-sensitivity":
                featureSensitivity = .high

            case "--normal-sensitivity":
                featureSensitivity = .normal

            case "--convert-tiffs":
                convertTIFFs = true

            case "--no-convert-tiffs":
                convertTIFFs = false

            case "--staging":
                stagingDirectory = URL(fileURLWithPath: try nextValue())
                    .standardizedFileURL

            case "--keep-staging":
                keepStagingDirectory = true

            case "-h", "--help":
                printUsage()
                Foundation.exit(0)

            default:
                throw CLIError.unknownArgument(argument)
            }

            index += 1
        }

        guard let inputDirectory else {
            throw CLIError.missingValue("--input")
        }

        guard let outputURL else {
            throw CLIError.missingValue("--output")
        }

        return CLIOptions(
            inputDirectory: inputDirectory,
            outputURL: outputURL,
            detail: detail,
            checkpointDirectory: checkpointDirectory,
            sampleOrdering: sampleOrdering,
            featureSensitivity: featureSensitivity,
            convertTIFFs: convertTIFFs,
            stagingDirectory: stagingDirectory,
            keepStagingDirectory: keepStagingDirectory
        )
    }

    private static func parseDetail(
        _ value: String
    ) throws -> PhotogrammetrySession.Request.Detail {
        switch value.lowercased() {
        case "preview":
            return .preview

        case "reduced":
            return .reduced

        case "medium":
            return .medium

        case "full":
            return .full

        case "raw":
            return .raw

        default:
            throw CLIError.invalidDetail(value)
        }
    }

    private static func validate(options: CLIOptions) throws {
        var isDirectory: ObjCBool = false

        guard FileManager.default.fileExists(
            atPath: options.inputDirectory.path,
            isDirectory: &isDirectory
        ), isDirectory.boolValue else {
            throw CLIError.invalidInputDirectory(options.inputDirectory.path)
        }

        let outputExtension = options.outputURL.pathExtension.lowercased()

        if outputExtension == "usdz" {
            let outputDirectory = options.outputURL.deletingLastPathComponent()

            try FileManager.default.createDirectory(
                at: outputDirectory,
                withIntermediateDirectories: true
            )
        } else {
            try FileManager.default.createDirectory(
                at: options.outputURL,
                withIntermediateDirectories: true
            )
        }

        let contents = try FileManager.default.contentsOfDirectory(
            at: options.inputDirectory,
            includingPropertiesForKeys: nil
        )

        let tiffImages = contents.filter { url in
            let ext = url.pathExtension.lowercased()
            return ext == "tif" || ext == "tiff" || ext == "jpg" || ext == "jpeg" 
        }

        guard !tiffImages.isEmpty else {
            throw CLIError.noTiffImages(options.inputDirectory.path)
        }
    }

    private static func printUsage() {
        print(
            """
            photomodel - RealityKit Object Capture CLI

            Usage:
              photomodel --input /path/to/tiffs --output /path/to/model.usdz [options]

            Required:
              -i, --input PATH          Folder containing .tif or .tiff photos
              -o, --output PATH         Output .usdz path

            Options:
              -d, --detail LEVEL        preview, reduced, medium, full, raw
                                        Default: medium

              --checkpoint PATH         Directory for RealityKit checkpoint data

              --unordered               Treat photos as unordered
                                        Default

              --sequential              Treat photos as sequentially captured

              --normal-sensitivity      Normal feature sensitivity
                                        Default

              --high-sensitivity        Use high feature sensitivity.
                                        Useful for low-texture objects.

              -h, --help                Show this help message

            Examples:
              photomodel \\
                --input ./photos \\
                --output ./model.usdz \\
                --detail full \\
                --checkpoint ./checkpoints

              photomodel \\
                -i /Users/peyton/object-tiffs \\
                -o /Users/peyton/Desktop/object.usdz \\
                -d medium \\
                --high-sensitivity
            """
        )
    }

    private static func prepareInputDirectory(options: CLIOptions) throws -> URL {
        let fileManager = FileManager.default

        let inputFiles = try fileManager.contentsOfDirectory(
            at: options.inputDirectory,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles]
        )
        .filter { url in
            let values = try? url.resourceValues(forKeys: [.isDirectoryKey])

            if values?.isDirectory == true {
                return false
            }

            return isSupportedInputImage(url)
        }
        .sorted { lhs, rhs in
            lhs.lastPathComponent.localizedStandardCompare(rhs.lastPathComponent)
                == .orderedAscending
        }

        let tiffFiles = inputFiles.filter(isTIFF)

        guard !inputFiles.isEmpty else {
            throw CLIError.noTiffImages(options.inputDirectory.path)
        }

        guard options.convertTIFFs, !tiffFiles.isEmpty else {
            return options.inputDirectory
        }

        let stagingDirectory = options.stagingDirectory
            ?? fileManager.temporaryDirectory.appendingPathComponent(
                "photomodel-staging-\(UUID().uuidString)",
                isDirectory: true
            )

        try fileManager.createDirectory(
            at: stagingDirectory,
            withIntermediateDirectories: true
        )

        print("Converting TIFFs to JPEG staging directory...")
        print("Staging: \(stagingDirectory.path)")

        for (index, inputURL) in inputFiles.enumerated() {
            let number = String(format: "%05d", index + 1)

            if isTIFF(inputURL) {
                let outputURL = stagingDirectory.appendingPathComponent(
                    "\(number).jpg"
                )

                try convertTIFFToJPEG(
                    inputURL: inputURL,
                    outputURL: outputURL,
                    quality: 0.96
                )
            } else {
                let outputURL = stagingDirectory.appendingPathComponent(
                    "\(number).\(inputURL.pathExtension.lowercased())"
                )

                if fileManager.fileExists(atPath: outputURL.path) {
                    try fileManager.removeItem(at: outputURL)
                }

                try fileManager.copyItem(at: inputURL, to: outputURL)
            }
        }

        return stagingDirectory
    }

    private static func isSupportedInputImage(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()

        return [
            "tif",
            "tiff",
            "jpg",
            "jpeg",
            "png",
            "heic",
            "heif"
        ].contains(ext)
    }

    private static func isTIFF(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()
        return ext == "tif" || ext == "tiff"
    }

    private static func convertTIFFToJPEG(
        inputURL: URL,
        outputURL: URL,
        quality: Double
    ) throws {
        let sourceOptions: [CFString: Any] = [
            kCGImageSourceShouldCache: false
        ]

        guard let source = CGImageSourceCreateWithURL(
            inputURL as CFURL,
            sourceOptions as CFDictionary
        ) else {
            throw CLIError.imageConversionFailed(
                "Could not open TIFF: \(inputURL.path)"
            )
        }

        guard CGImageSourceGetCount(source) > 0 else {
            throw CLIError.imageConversionFailed(
                "TIFF contains no images: \(inputURL.path)"
            )
        }

        guard let destination = CGImageDestinationCreateWithURL(
            outputURL as CFURL,
            UTType.jpeg.identifier as CFString,
            1,
            nil
        ) else {
            throw CLIError.imageConversionFailed(
                "Could not create JPEG destination: \(outputURL.path)"
            )
        }

        let destinationProperties: [CFString: Any] = [
            kCGImageDestinationLossyCompressionQuality: quality
        ]

        CGImageDestinationAddImageFromSource(
            destination,
            source,
            0,
            destinationProperties as CFDictionary
        )

        guard CGImageDestinationFinalize(destination) else {
            throw CLIError.imageConversionFailed(
                "Could not write JPEG: \(outputURL.path)"
            )
        }
    }

    private static func formatDuration(_ seconds: TimeInterval) -> String {
        let totalSeconds = Int(seconds.rounded())
        let hours = totalSeconds / 3600
        let minutes = (totalSeconds % 3600) / 60
        let seconds = totalSeconds % 60

        if hours > 0 {
            return String(format: "%dh %02dm %02ds", hours, minutes, seconds)
        }

        if minutes > 0 {
            return String(format: "%dm %02ds", minutes, seconds)
        }

        return String(format: "%ds", seconds)
    }
}