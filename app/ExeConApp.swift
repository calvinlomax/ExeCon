import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let execonPE = UTType(importedAs: "com.microsoft.windows-executable")
}

@main
struct ExeConApp: App {
    @StateObject private var runner = Runner()
    var body: some Scene {
        WindowGroup("ExeCon") {
            ContentView(runner: runner)
                .onOpenURL { runner.select($0) }
                .frame(minWidth: 880, minHeight: 620)
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Open Executable…") { runner.openPanel() }.keyboardShortcut("o")
            }
        }
    }
}

struct ContentView: View {
    @ObservedObject var runner: Runner
    private func color(_ code: Int?) -> Color {
        switch code {
        case 30, 90: return .secondary
        case 31, 91: return .red
        case 32, 92: return .green
        case 33, 93: return .yellow
        case 34, 94: return .blue
        case 35, 95: return .purple
        case 36, 96: return .cyan
        default: return .primary
        }
    }
    private var consoleText: Text {
        runner.console.reduce(Text("")) { result, run in result + Text(run.text).foregroundColor(color(run.color)) }
    }
    var body: some View {
        NavigationSplitView {
            List {
                Section("Program") {
                    Button("Open Executable…", systemImage: "folder") { runner.openPanel() }
                    if let program = runner.program {
                        Label(program.lastPathComponent, systemImage: "terminal")
                        Text(program.path).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                    } else { Text("Drop a .exe here to begin.").foregroundStyle(.secondary) }
                }
                Section("Recent runs") {
                    ForEach(runner.history) { entry in
                        Button { runner.select(URL(fileURLWithPath: entry.path)) } label: {
                            VStack(alignment: .leading) {
                                Text(URL(fileURLWithPath: entry.path).lastPathComponent)
                                Text("\(entry.date.formatted(date: .abbreviated, time: .shortened)) · status \(entry.status)")
                                    .font(.caption2).foregroundStyle(.secondary)
                            }
                        }.buttonStyle(.plain)
                    }
                }
            }.navigationSplitViewColumnWidth(min: 210, ideal: 250)
        } detail: {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    VStack(alignment: .leading) {
                        Text(runner.program?.lastPathComponent ?? "ExeCon").font(.title2).bold()
                        Text(runner.status).foregroundStyle(.secondary).font(.callout)
                    }
                    Spacer()
                    Button("Run", systemImage: "play.fill") { runner.run() }.disabled(runner.program == nil || runner.isRunning)
                    Button("Stop", systemImage: "stop.fill") { runner.stop() }.disabled(!runner.isRunning)
                    Button("Restart", systemImage: "arrow.clockwise") { runner.restart() }.disabled(runner.program == nil)
                }
                TabView {
                    VStack(spacing: 0) {
                        ScrollView([.vertical, .horizontal]) {
                            consoleText.font(.system(.body, design: .monospaced)).textSelection(.enabled)
                                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading).padding(12)
                        }.background(.background.secondary)
                        HStack {
                            TextField("Send input to the running program", text: $runner.input).onSubmit { runner.sendInput() }
                            Button("Send") { runner.sendInput() }.disabled(!runner.isRunning)
                        }.padding(8)
                    }.tabItem { Label("Console", systemImage: "terminal") }
                    Form {
                        LabeledContent("Arguments (one per line)") { TextEditor(text: $runner.settings.arguments).frame(height: 64).font(.system(.body, design: .monospaced)) }
                        TextField("Working directory", text: $runner.settings.workingDirectory)
                        TextField("Windows prefix", text: $runner.settings.prefix)
                        LabeledContent("Environment (KEY=VALUE per line)") { TextEditor(text: $runner.settings.environment).frame(height: 64).font(.system(.body, design: .monospaced)) }
                        Picker("Trace", selection: $runner.settings.trace) { ForEach(["off", "cpu", "api", "all"], id: \.self) { Text($0).tag($0) } }
                        Picker("Missing APIs", selection: $runner.settings.missingPolicy) {
                            Text("Stop with a diagnostic").tag("abort")
                            Text("Return an error where supported").tag("return-error")
                            Text("Ignore documented void stubs").tag("ignore-void")
                        }
                        Toggle("Use interpreter only", isOn: $runner.settings.interpreterOnly)
                    }.formStyle(.grouped).disabled(runner.isRunning).tabItem { Label("Program Settings", systemImage: "slider.horizontal.3") }
                    ScrollView {
                        Text(runner.diagnostics.isEmpty ? "Diagnostics and traces appear here." : runner.diagnostics)
                            .font(.system(.body, design: .monospaced)).textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .topLeading).padding()
                    }.tabItem { Label("Log & Trace", systemImage: "doc.text.magnifyingglass") }
                }
                HStack {
                    Image(systemName: "info.circle")
                    Text("Experimental console support. Most Windows applications are not supported yet.")
                }.font(.caption).foregroundStyle(.secondary)
                Text(runner.toolingStatus).font(.caption2).foregroundStyle(.secondary).textSelection(.enabled)
            }.padding(20)
        }
        .overlay { if runner.dropTarget { RoundedRectangle(cornerRadius: 12).stroke(.blue, lineWidth: 3).padding(8) } }
        .onDrop(of: [UTType.fileURL], isTargeted: $runner.dropTarget) { providers in
            guard let provider = providers.first else { return false }
            _ = provider.loadDataRepresentation(forTypeIdentifier: UTType.fileURL.identifier) { data, _ in
                if let data, let url = URL(dataRepresentation: data, relativeTo: nil) {
                    Task { @MainActor in runner.select(url) }
                }
            }
            return true
        }
    }
}
