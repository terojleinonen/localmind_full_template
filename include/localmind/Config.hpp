#pragma once
#include <string>
#include <vector>

namespace localmind {

struct Config {
    // HTTP
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string webDir = "web";       // static UI; empty disables it
    std::string corsOrigin;           // empty = same-origin only
    std::size_t maxBodyBytes = 10 * 1024 * 1024;
    int threads = 8;

    // Storage
    std::string dbPath = "data/db/localmind.db";
    std::vector<std::string> ingestPaths; // files/dirs indexed at startup

    // Retrieval
    std::string embedder = "hashing";     // "hashing" | "ollama"
    std::size_t chunkSize = 800;          // characters
    std::size_t chunkOverlap = 120;       // characters
    int topK = 4;
    float minScore = 0.05f;

    // Generation
    std::string llm = "ollama";           // "ollama" | "none"
    std::string ollamaUrl = "http://127.0.0.1:11434";
    std::string embedModel = "nomic-embed-text";
    std::string chatModel = "llama3.2";
    int ollamaTimeoutSec = 120;

    std::string logLevel = "info";
};

// Result of parsing command-line arguments.
struct ParseResult {
    bool ok = true;
    bool exitNow = false;   // --help / --version were handled
    std::string message;    // error or help text
};

// Applies LOCALMIND_* / OLLAMA_* environment variables, then argv flags.
// Precedence: defaults < environment < command line.
ParseResult parseConfig(int argc, const char* const argv[], Config& cfg);

std::string usage(const std::string& prog);

} // namespace localmind
