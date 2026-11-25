// Finds only the sandbox process window and optionally applies accessibility window actions.
// Missing screen/accessibility permissions are reported, never requested or bypassed.
import Cocoa
import ApplicationServices

let pid = pid_t(CommandLine.arguments[1])!
let operation = CommandLine.arguments[2]
// Quartz input constructors: https://developer.apple.com/documentation/coregraphics/cgevent
if ["orbit", "zoom", "reset"].contains(operation) {
    let list = CGWindowListCopyWindowInfo([.optionAll, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
    guard let window = list.first(where: { ($0[kCGWindowOwnerPID as String] as? Int) == Int(pid) && ($0[kCGWindowLayer as String] as? Int) == 0 }),
          let bounds = window[kCGWindowBounds as String] as? [String: Any],
          let rect = CGRect(dictionaryRepresentation: bounds as CFDictionary) else { exit(77) }
    NSRunningApplication(processIdentifier: pid)?.activate(options: [.activateIgnoringOtherApps])
    Thread.sleep(forTimeInterval: 0.2)
    var point = CGPoint(x: rect.minX + rect.width * 0.6, y: rect.minY + rect.height * 0.6)
    func mouse(_ type: CGEventType, _ point: CGPoint, dx: Int64 = 0, dy: Int64 = 0) {
        let event = CGEvent(mouseEventSource: nil, mouseType: type, mouseCursorPosition: point, mouseButton: .right)!
        event.setIntegerValueField(.mouseEventDeltaX, value: dx)
        event.setIntegerValueField(.mouseEventDeltaY, value: dy)
        event.postToPid(pid)
    }
    if operation == "orbit" {
        mouse(.mouseMoved, point)
        mouse(.rightMouseDown, point)
        for _ in 0..<12 {
            point.x += 5; point.y -= 2
            mouse(.rightMouseDragged, point, dx: 5, dy: -2)
            Thread.sleep(forTimeInterval: 0.05)
        }
        mouse(.rightMouseUp, point)
    } else if operation == "zoom" {
        for _ in 0..<2 {
            CGEvent(scrollWheelEvent2Source: nil, units: .line, wheelCount: 1, wheel1: 1, wheel2: 0, wheel3: 0)!.postToPid(pid)
            Thread.sleep(forTimeInterval: 0.15)
        }
    } else {
        CGEvent(keyboardEventSource: nil, virtualKey: 15, keyDown: true)!.postToPid(pid)
        CGEvent(keyboardEventSource: nil, virtualKey: 15, keyDown: false)!.postToPid(pid)
    }
    exit(0)
}
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
