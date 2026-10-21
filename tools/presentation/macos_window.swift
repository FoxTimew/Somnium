// Finds only the sandbox process window and optionally applies accessibility window actions.
// Missing screen/accessibility permissions are reported, never requested or bypassed.
import Cocoa
import ApplicationServices

let pid = pid_t(CommandLine.arguments[1])!
let operation = CommandLine.arguments[2]
if operation == "inspect" {
    let windows = CGWindowListCopyWindowInfo([.optionAll, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
    let matches = windows.filter {
        ($0[kCGWindowOwnerPID as String] as? Int) == Int(pid) &&
        ($0[kCGWindowLayer as String] as? Int) == 0
    }
    let data = try! JSONSerialization.data(withJSONObject: matches)
    print(String(data: data, encoding: .utf8)!)
} else {
    let application = AXUIElementCreateApplication(pid)
    var value: CFTypeRef?
    let query = AXUIElementCopyAttributeValue(application, kAXWindowsAttribute as CFString, &value)
    guard query == .success, let windows = value as? [AXUIElement], let window = windows.first else {
        fputs("Window accessibility unavailable: \(query.rawValue)\n", stderr)
        exit(77)
    }
    let result: AXError
    if operation == "resize" {
        var size = CGSize(width: 960, height: 600)
        result = AXUIElementSetAttributeValue(window, kAXSizeAttribute as CFString, AXValueCreate(.cgSize, &size)!)
    } else {
        result = AXUIElementSetAttributeValue(window, kAXMinimizedAttribute as CFString,
                                              operation == "minimize" ? kCFBooleanTrue : kCFBooleanFalse)
    }
    guard result == .success else {
        fputs("Window action unavailable: \(result.rawValue)\n", stderr)
        exit(77)
    }
}
