#include "localmind/Config.hpp"

#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>

namespace localmind {

namespace {

int toInt(const std::string& name, const std::string& v, int lo, int hi) {
    std::size_t pos = 0;
    int n = 0;
    try {
        n = std::stoi(v, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos != v.size() || v.empty())
        throw std::invalid_argument(name + ": expected an integer, got '" + v + "'");
    if (n < lo || n > hi)
        throw std::invalid_argument(name + ": must be between " + std::to_string(lo) +
                                    " and " + std::to_string(hi));
    return n;
}

float toFloat(const std::string& name, const std::string& v) {
    std::size_t pos = 0;
    float f = 0;
    try {
        f = std::stof(v, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos != v.size() || v.empty())
        throw std::invalid_argument(name + ": expected a number, got '" + v + "'");
    return f;
}

void oneOf(const std::string& name, const std::string& v,
           std::initializer_list<const char*> allowed) {
    for (auto* a : allowed)
        if (v == a) return;
    std::string list;
    for (auto* a : allowed) list += std::string(list.empty() ? "" : ", ") + a;
    throw std::invalid_argument(name + ": must be one of " + list);
}

using Setter = std::function<void(Config&, const std::string&)>;

struct Option {
    const char* flag;
    const char* env;
    const char* help;
    Setter set;
};

const std::vector<Option>& options() {
    static const std::vector<Option> opts = {
        {"--host", "LOCALMIND_HOST", "Address to bind (default 127.0.0.1)",
         [](Config& c, const std::string& v) { c.host = v; }},
        {"--port", "LOCALMIND_PORT", "Port to listen on (default 8080, 0 = any)",
         [](Config& c, const std::string& v) { c.port = toInt("port", v, 0, 65535); }},
        {"--web-dir", "LOCALMIND_WEB_DIR", "Directory with the web UI; '' disables it (default web)",
         [](Config& c, const std::string& v) { c.webDir = v; }},
        {"--cors-origin", "LOCALMIND_CORS_ORIGIN", "Allowed CORS origin, e.g. http://localhost:3000",
         [](Config& c, const std::string& v) { c.corsOrigin = v; }},
        {"--max-body-mb", "LOCALMIND_MAX_BODY_MB", "Max request body in MiB (default 10)",
         [](Config& c, const std::string& v) {
             c.maxBodyBytes = static_cast<std::size_t>(toInt("max-body-mb", v, 1, 1024)) * 1024 * 1024;
         }},
        {"--threads", "LOCALMIND_THREADS", "HTTP worker threads (default 8)",
         [](Config& c, const std::string& v) { c.threads = toInt("threads", v, 1, 256); }},
        {"--db", "LOCALMIND_DB", "SQLite database path (default data/db/localmind.db)",
         [](Config& c, const std::string& v) { c.dbPath = v; }},
        {"--ingest", "LOCALMIND_INGEST", "File or directory to index at startup (repeatable)",
         [](Config& c, const std::string& v) { c.ingestPaths.push_back(v); }},
        {"--embedder", "LOCALMIND_EMBEDDER", "hashing | ollama (default hashing)",
         [](Config& c, const std::string& v) { oneOf("embedder", v, {"hashing", "ollama"}); c.embedder = v; }},
        {"--chunk-size", "LOCALMIND_CHUNK_SIZE", "Chunk size in characters (default 800)",
         [](Config& c, const std::string& v) { c.chunkSize = toInt("chunk-size", v, 64, 100000); }},
        {"--chunk-overlap", "LOCALMIND_CHUNK_OVERLAP", "Chunk overlap in characters (default 120)",
         [](Config& c, const std::string& v) { c.chunkOverlap = toInt("chunk-overlap", v, 0, 50000); }},
        {"--top-k", "LOCALMIND_TOP_K", "Passages retrieved per question (default 4)",
         [](Config& c, const std::string& v) { c.topK = toInt("top-k", v, 1, 50); }},
        {"--min-score", "LOCALMIND_MIN_SCORE", "Minimum cosine similarity (default 0.05)",
         [](Config& c, const std::string& v) { c.minScore = toFloat("min-score", v); }},
        {"--llm", "LOCALMIND_LLM", "ollama | none (default ollama, falls back to extractive)",
         [](Config& c, const std::string& v) { oneOf("llm", v, {"ollama", "none"}); c.llm = v; }},
        {"--ollama-url", "OLLAMA_URL", "Ollama base URL (default http://127.0.0.1:11434)",
         [](Config& c, const std::string& v) { c.ollamaUrl = v; }},
        {"--embed-model", "LOCALMIND_EMBED_MODEL", "Ollama embedding model (default nomic-embed-text)",
         [](Config& c, const std::string& v) { c.embedModel = v; }},
        {"--chat-model", "LOCALMIND_CHAT_MODEL", "Ollama chat model (default llama3.2)",
         [](Config& c, const std::string& v) { c.chatModel = v; }},
        {"--ollama-timeout", "LOCALMIND_OLLAMA_TIMEOUT", "Ollama request timeout in seconds (default 120)",
         [](Config& c, const std::string& v) { c.ollamaTimeoutSec = toInt("ollama-timeout", v, 1, 3600); }},
        {"--log-level", "LOCALMIND_LOG_LEVEL", "debug | info | warn | error (default info)",
         [](Config& c, const std::string& v) { oneOf("log-level", v, {"debug", "info", "warn", "warning", "error"}); c.logLevel = v; }},
    };
    return opts;
}

} // namespace

std::string usage(const std::string& prog) {
    std::ostringstream out;
    out << "LocalMind " << LOCALMIND_VERSION << " - local document Q&A server\n\n"
        << "Usage: " << prog << " [options]\n\nOptions (environment variable in brackets):\n";
    for (const auto& o : options()) {
        std::string flag = std::string("  ") + o.flag + " <value>";
        out << flag << std::string(flag.size() < 28 ? 28 - flag.size() : 1, ' ')
            << o.help << " [" << o.env << "]\n";
    }
    out << "  -h, --help                Show this help\n"
        << "  -v, --version             Show version\n";
    return out.str();
}

ParseResult parseConfig(int argc, const char* const argv[], Config& cfg) {
    ParseResult r;
    const std::string prog = argc > 0 ? argv[0] : "localmind_server";
    try {
        for (const auto& o : options()) {
            if (const char* v = std::getenv(o.env)) {
                // LOCALMIND_INGEST may hold several paths separated by ':'.
                if (std::string(o.env) == "LOCALMIND_INGEST") {
                    std::stringstream ss(v);
                    std::string part;
                    while (std::getline(ss, part, ':'))
                        if (!part.empty()) o.set(cfg, part);
                } else {
                    o.set(cfg, v);
                }
            }
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                r.exitNow = true;
                r.message = usage(prog);
                return r;
            }
            if (arg == "-v" || arg == "--version") {
                r.exitNow = true;
                r.message = std::string("LocalMind ") + LOCALMIND_VERSION + "\n";
                return r;
            }

            std::string value;
            bool hasValue = false;
            auto eq = arg.find('=');
            if (eq != std::string::npos) {
                value = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
                hasValue = true;
            }

            const Option* match = nullptr;
            for (const auto& o : options())
                if (arg == o.flag) match = &o;
            if (!match)
                throw std::invalid_argument("unknown option '" + arg + "' (see --help)");

            if (!hasValue) {
                if (i + 1 >= argc)
                    throw std::invalid_argument(arg + " requires a value");
                value = argv[++i];
            }
            match->set(cfg, value);
        }

        if (cfg.chunkOverlap >= cfg.chunkSize)
            throw std::invalid_argument("chunk-overlap must be smaller than chunk-size");
    } catch (const std::invalid_argument& e) {
        r.ok = false;
        r.message = e.what();
    }
    return r;
}

} // namespace localmind
