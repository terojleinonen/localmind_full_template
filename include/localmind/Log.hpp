#pragma once
#include <string>

namespace localmind {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// Minimal thread-safe logger writing timestamped lines to stderr.
class Log {
public:
    static void setLevel(LogLevel level);
    static LogLevel level();
    // Parses "debug" / "info" / "warn" / "error"; returns false if unknown.
    static bool parseLevel(const std::string& name, LogLevel& out);

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);

private:
    static void write(LogLevel level, const std::string& msg);
};

} // namespace localmind
