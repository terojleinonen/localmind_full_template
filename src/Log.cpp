#include "localmind/Log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace localmind {

namespace {
std::atomic<int> g_level{static_cast<int>(LogLevel::Info)};
std::mutex g_mu;

const char* levelName(LogLevel l) {
    switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}
} // namespace

void Log::setLevel(LogLevel level) { g_level = static_cast<int>(level); }
LogLevel Log::level() { return static_cast<LogLevel>(g_level.load()); }

bool Log::parseLevel(const std::string& name, LogLevel& out) {
    if (name == "debug") out = LogLevel::Debug;
    else if (name == "info") out = LogLevel::Info;
    else if (name == "warn" || name == "warning") out = LogLevel::Warn;
    else if (name == "error") out = LogLevel::Error;
    else return false;
    return true;
}

void Log::debug(const std::string& msg) { write(LogLevel::Debug, msg); }
void Log::info(const std::string& msg) { write(LogLevel::Info, msg); }
void Log::warn(const std::string& msg) { write(LogLevel::Warn, msg); }
void Log::error(const std::string& msg) { write(LogLevel::Error, msg); }

void Log::write(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < g_level.load()) return;

    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tm{};
    gmtime_r(&t, &tm);
    char ts[32];
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S", &tm);

    std::lock_guard<std::mutex> lock(g_mu);
    std::fprintf(stderr, "%s.%03dZ %s %s\n", ts, static_cast<int>(ms),
                 levelName(level), msg.c_str());
    std::fflush(stderr);
}

} // namespace localmind
