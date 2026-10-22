import Foundation

struct ConsoleRun: Equatable {
    var text: String
    var color: Int?
}

// Streaming UTF-8 and SGR parser. Escape sequences and Unicode scalars may span pipe reads.
struct ConsoleDecoder {
    private var pending: [UInt8] = []
    private var color: Int?

    mutating func append(_ data: Data, final: Bool = false) -> [ConsoleRun] {
        pending.append(contentsOf: data)
        var runs: [ConsoleRun] = []
        var index = 0
        func emit(_ text: String, color: Int?) {
            if runs.last?.color == color, !runs.isEmpty {
                runs[runs.count - 1].text += text
            } else { runs.append(ConsoleRun(text: text, color: color)) }
        }
        while index < pending.count {
            let byte = pending[index]
            if byte == 0x1b {
                if index + 1 >= pending.count { break }
                if pending[index + 1] != 0x5b { index += 2; continue }
                var end = index + 2
                while end < pending.count && !(0x40...0x7e).contains(pending[end]) && end - index < 64 { end += 1 }
                if end == pending.count && end - index < 64 { break }
                if end < pending.count && pending[end] == 0x6d {
                    let parameters = String(decoding: pending[(index + 2)..<end], as: UTF8.self)
                    for code in parameters.split(separator: ";", omittingEmptySubsequences: false) {
                        let value = Int(code) ?? 0
                        if value == 0 || value == 39 { color = nil }
                        else if (30...37).contains(value) || (90...97).contains(value) { color = value }
                    }
                }
                index = min(end + 1, pending.count)
                continue
            }
            let length: Int
            if byte < 0x80 { length = 1 }
            else if (0xc2...0xdf).contains(byte) { length = 2 }
            else if (0xe0...0xef).contains(byte) { length = 3 }
            else if (0xf0...0xf4).contains(byte) { length = 4 }
            else { emit("�", color: color); index += 1; continue }
            if pending.count - index < length { break }
            let slice = pending[index..<(index + length)]
            if let text = String(bytes: slice, encoding: .utf8) {
                // Basic text console: retain newline/tab, suppress other C0 control bytes.
                if length > 1 || byte >= 0x20 || byte == 10 || byte == 9 { emit(text, color: color) }
                index += length
            } else { emit("�", color: color); index += 1 }
        }
        pending.removeFirst(index)
        if final && !pending.isEmpty {
            if pending.first != 0x1b { emit(String(decoding: pending, as: UTF8.self), color: color) }
            pending.removeAll()
        }
        return runs
    }
}
