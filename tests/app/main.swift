import Foundation
var decoder = ConsoleDecoder()
let bytes = Array("plain \u{1b}[31mred λ 🚀\u{1b}[0m end\n".utf8)
var runs: [ConsoleRun] = []
for byte in bytes { runs += decoder.append(Data([byte])) }
runs += decoder.append(Data(), final: true)
precondition(runs.map(\.text).joined() == "plain red λ 🚀 end\n")
precondition(runs.filter { $0.color == 31 }.map(\.text).joined() == "red λ 🚀")
var invalid = ConsoleDecoder()
precondition(invalid.append(Data([0xff])).map(\.text).joined() == "�")
precondition(invalid.append(Data([0xe2])).isEmpty)
precondition(invalid.append(Data(), final: true).map(\.text).joined() == "�")
var escapes = ConsoleDecoder()
precondition(escapes.append(Data("\u{1b}[2Jtext".utf8)).map(\.text).joined() == "text")
print("PASS streaming UTF-8, split ANSI SGR, reset, malformed UTF-8 and CSI suppression")
