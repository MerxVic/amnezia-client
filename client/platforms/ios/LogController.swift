/Users/vopekunov/.zprofile:2: no such file or directory: /opt/homebrew/bin/brew
import Foundation

private func cxxString(_ value: String) -> std.string {
  var result = std.string()
  for byte in value.utf8 {
    result.push_back(CChar(bitPattern: byte))
  }
  return result
}

public func swiftAppLogPath() -> std.string {
  return cxxString(Log.appLogURL.path)
}

public func swiftTunnelLogPath() -> std.string {
  return cxxString(Log.neLogURL.path)
}

public func toggleLogging(_ isEnabled: Bool) {
  Log.isLoggingEnabled = isEnabled
}
