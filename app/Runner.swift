import AppKit
import Foundation
import SwiftUI

struct ProgramSettings: Codable, Equatable {
    var arguments = ""
    var workingDirectory = "C:\\"
    var environment = ""
    var prefix = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/ExeCon/prefix").path
    var trace = "off"
    var missingPolicy = "abort"
    var interpreterOnly = false
}
struct HistoryEntry: Codable, Identifiable {
    var id = UUID()
    var path: String
    var date: Date
    var status: Int32
}

@MainActor
final class Runner: ObservableObject {
    @Published var dropTarget = false
    @Published var program: URL?
    @Published var settings = ProgramSettings() { didSet { saveSettings() } }
    @Published var console: [ConsoleRun] = []
    @Published var diagnostics = ""
    @Published var input = ""
    @Published var status = "Choose a Windows console executable"
    @Published var isRunning = false
    @Published var history: [HistoryEntry] = []
    @Published var toolingStatus = "Checking execution host…"
    private var process: Process?
    private var inputPipe: Pipe?
    private var outputPipe: Pipe?
    private var errorPipe: Pipe?
    private var stdoutDecoder = ConsoleDecoder()
    private var stderrDecoder = ConsoleDecoder()
    private var restartAfterExit = false
    private var runID = UUID()
    private var preferences: [String: ProgramSettings] = [:]
    private let defaults = UserDefaults.standard

    init() {
        if let data = defaults.data(forKey: "programSettings"),
           let decoded = try? JSONDecoder().decode([String: ProgramSettings].self, from: data) { preferences = decoded }
        if let data = defaults.data(forKey: "history"),
           let decoded = try? JSONDecoder().decode([HistoryEntry].self, from: data) { history = decoded }
        checkHost()
    }
    var hostURL: URL { Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/execon") }
    var engineVersion: String { String(cString: execon_version()) }

    func openPanel() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [.execonPE]
        if panel.runModal() == .OK, let url = panel.url { select(url) }
    }
    func select(_ url: URL) {
        guard !isRunning else { status = "Stop the current program before opening another."; return }
        guard url.pathExtension.lowercased() == "exe" else { status = "Choose a .exe file."; return }
        program = url
        settings = preferences[url.path] ?? ProgramSettings()
        status = "Ready: \(url.lastPathComponent)"
    }
    private func saveSettings() {
        guard let program else { return }
        preferences[program.path] = settings
        if let data = try? JSONEncoder().encode(preferences) { defaults.set(data, forKey: "programSettings") }
    }
    private func checkHost() {
        guard FileManager.default.isExecutableFile(atPath: hostURL.path) else {
            toolingStatus = "Execution host missing. Rebuild with scripts/build.sh and open the resulting ExeCon.app."
            return
        }
        let check = Process(), pipe = Pipe()
        check.executableURL = hostURL
        check.arguments = ["--self-check"]
        check.standardOutput = pipe
        check.standardError = pipe
        check.terminationHandler = { [weak self] child in
            let output = String(decoding: pipe.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
            Task { @MainActor in
                self?.toolingStatus = child.terminationStatus == 0 ? "Execution host ready · Engine \(self?.engineVersion ?? "")" : "JIT check failed. Rebuild or run scripts/sign.sh on the bundled execon host. \(output)"
            }
        }
        do { try check.run() } catch { toolingStatus = "Cannot launch execution host: \(error.localizedDescription)" }
    }
    func run() {
        guard let program, !isRunning else { return }
        console = []; diagnostics = ""; stdoutDecoder = ConsoleDecoder(); stderrDecoder = ConsoleDecoder()
        let child = Process(), stdout = Pipe(), stderr = Pipe(), stdin = Pipe()
        let identifier = UUID(); runID = identifier
        child.executableURL = hostURL
        var arguments = ["--prefix", settings.prefix, "--cwd", settings.workingDirectory,
                         "--on-missing=\(settings.missingPolicy)", "--stats"]
        if settings.interpreterOnly { arguments.append("--interp-only") }
        if settings.trace != "off" { arguments.append("--trace=\(settings.trace)") }
        for entry in settings.environment.split(separator: "\n") { arguments += ["--env", String(entry)] }
        arguments += [program.path]
        if !settings.arguments.isEmpty { arguments += settings.arguments.components(separatedBy: "\n") }
        child.arguments = arguments
        child.standardOutput = stdout; child.standardError = stderr; child.standardInput = stdin
        stdout.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if data.isEmpty { handle.readabilityHandler = nil }
            Task { @MainActor in self?.receive(data, isError: false, id: identifier, final: data.isEmpty) }
        }
        stderr.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if data.isEmpty { handle.readabilityHandler = nil }
            Task { @MainActor in self?.receive(data, isError: true, id: identifier, final: data.isEmpty) }
        }
        child.terminationHandler = { [weak self] terminated in
            Task { @MainActor in
                guard let self, self.runID == identifier else { return }
                self.isRunning = false
                self.status = terminated.terminationReason == .uncaughtSignal ? "Stopped (signal \(terminated.terminationStatus))" : "Exited with status \(terminated.terminationStatus)"
                self.history.insert(HistoryEntry(path: program.path, date: Date(), status: terminated.terminationStatus), at: 0)
                self.history = Array(self.history.prefix(20))
                if let data = try? JSONEncoder().encode(self.history) { self.defaults.set(data, forKey: "history") }
                self.process = nil; self.inputPipe = nil
                if self.restartAfterExit { self.restartAfterExit = false; self.run() }
            }
        }
        process = child; outputPipe = stdout; errorPipe = stderr; inputPipe = stdin
        do { try child.run(); isRunning = true; status = "Running \(program.lastPathComponent)" }
        catch { stdout.fileHandleForReading.readabilityHandler = nil; stderr.fileHandleForReading.readabilityHandler = nil; process = nil; inputPipe = nil; status = "Launch failed: \(error.localizedDescription)" }
    }
    private func receive(_ data: Data, isError: Bool, id: UUID, final: Bool) {
        guard runID == id else { return }
        let runs = isError ? stderrDecoder.append(data, final: final) : stdoutDecoder.append(data, final: final)
        console += runs
        if isError { diagnostics += runs.map(\.text).joined() }
        // Bounded display memory; the engine itself still writes the full stream.
        while console.reduce(0, { $0 + $1.text.utf8.count }) > 512 * 1024, !console.isEmpty { console.removeFirst() }
        if diagnostics.count > 512 * 1024 { diagnostics = String(diagnostics.suffix(256 * 1024)) }
    }
    func sendInput() {
        guard isRunning, let pipe = inputPipe else { return }
        do { try pipe.fileHandleForWriting.write(contentsOf: Data((input + "\n").utf8)); input = "" }
        catch { status = "Input closed: \(error.localizedDescription)" }
    }
    func stop() { process?.terminate() }
    func restart() { if isRunning { restartAfterExit = true; stop() } else { run() } }
}
