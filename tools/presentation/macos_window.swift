// Reports visible sandbox windows without activation, accessibility actions or input events.
// The caller chooses the largest surface, excluding auxiliary menu windows.
import Cocoa
let pid = pid_t(CommandLine.arguments[1])!
let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
let matches = windows.filter {
    ($0[kCGWindowOwnerPID as String] as? Int) == Int(pid) &&
    ($0[kCGWindowLayer as String] as? Int) == 0
}
let data = try! JSONSerialization.data(withJSONObject: matches)
print(String(data: data, encoding: .utf8)!)
