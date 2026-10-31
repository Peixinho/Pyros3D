// photos_to_model: a folder of photographs of one thing, taken from all round
// it, into a textured model (Apple's Object Capture; macOS 12 or later).
//
//   swiftc -O photos_to_model.swift -o photos_to_model
//   ./photos_to_model <folder of photos> <out.usdz> [preview|reduced|medium|full|raw]
//
// A video does as well: ffmpeg -i head.mov -vf fps=3 -q:v 2 photos/f_%04d.jpg
import Foundation
import RealityKit

let args = CommandLine.arguments
guard args.count >= 3 else {
    FileHandle.standardError.write("usage: photos_to_model <folder of photos> <out.usdz> [preview|reduced|medium|full|raw]\n".data(using: .utf8)!)
    exit(2)
}
let input = URL(fileURLWithPath: args[1], isDirectory: true)
let output = URL(fileURLWithPath: args[2])
let detailName = args.count > 3 ? args[3] : "medium"
let details: [String: PhotogrammetrySession.Request.Detail] = ["preview": .preview, "reduced": .reduced, "medium": .medium, "full": .full, "raw": .raw]
guard let detail = details[detailName] else {
    FileHandle.standardError.write("unknown detail '\(detailName)'\n".data(using: .utf8)!)
    exit(2)
}
guard PhotogrammetrySession.isSupported else {
    FileHandle.standardError.write("Object Capture is not supported on this machine\n".data(using: .utf8)!)
    exit(1)
}

var configuration = PhotogrammetrySession.Configuration()
configuration.featureSensitivity = .high        // skin has little to hold on to
configuration.sampleOrdering = .unordered

do {
    let session = try PhotogrammetrySession(input: input, configuration: configuration)
    let waiter = Task {
        do {
            for try await out in session.outputs {
                switch out {
                case .requestProgress(_, let fraction):
                    print(String(format: "progress %.0f%%", fraction * 100))
                case .requestComplete(_, _):
                    print("wrote \(output.path)")
                case .requestError(_, let error):
                    FileHandle.standardError.write("failed: \(error)\n".data(using: .utf8)!)
                    exit(1)
                case .invalidSample(let id, let reason):
                    print("photo \(id) left out: \(reason)")
                case .processingComplete:
                    exit(0)
                case .processingCancelled:
                    exit(1)
                default:
                    break
                }
            }
        } catch {
            FileHandle.standardError.write("failed: \(error)\n".data(using: .utf8)!)
            exit(1)
        }
    }
    _ = waiter
    try session.process(requests: [.modelFile(url: output, detail: detail)])
    RunLoop.main.run()
} catch {
    FileHandle.standardError.write("failed: \(error)\n".data(using: .utf8)!)
    exit(1)
}
