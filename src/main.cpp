#include <csignal>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <thread>

#include "localmind/Config.hpp"
#include "localmind/Embedder.hpp"
#include "localmind/LlmClient.hpp"
#include "localmind/Log.hpp"
#include "localmind/QueryEngine.hpp"
#include "localmind/Server.hpp"
#include "localmind/VectorStore.hpp"

using namespace localmind;

int main(int argc, char** argv) {
    Config cfg;
    auto parsed = parseConfig(argc, argv, cfg);
    if (!parsed.ok) {
        std::fprintf(stderr, "error: %s\n", parsed.message.c_str());
        return 2;
    }
    if (parsed.exitNow) {
        std::fputs(parsed.message.c_str(), stdout);
        return 0;
    }

    LogLevel level;
    if (Log::parseLevel(cfg.logLevel, level)) Log::setLevel(level);

    // Block SIGINT/SIGTERM in every thread; a dedicated thread waits for them
    // and shuts the server down cleanly (no work inside a signal handler).
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    try {
        Log::info(std::string("LocalMind ") + LOCALMIND_VERSION + " starting");

        auto embedder = makeEmbedder(cfg);
        VectorStore store(cfg.dbPath);
        auto llm = makeLlmClient(cfg);
        QueryEngine engine(*embedder, store, llm.get(),
                           {cfg.chunkSize, cfg.chunkOverlap, cfg.topK, cfg.minScore});

        engine.syncEmbeddings();
        for (const auto& path : cfg.ingestPaths) {
            auto results = engine.indexPath(path);
            std::size_t added = 0;
            for (const auto& r : results) added += r.duplicate ? 0 : 1;
            Log::info("Ingested " + path + ": " + std::to_string(added) + " new, " +
                      std::to_string(results.size() - added) + " already indexed");
        }

        ServerApp server(cfg, engine);
        int port = server.bind();
        Log::info("Listening on http://" + cfg.host + ":" + std::to_string(port));

        std::thread waiter([&server, signals] {
            int sig = 0;
            sigwait(&signals, &sig);
            Log::info(std::string("Received ") + (sig == SIGINT ? "SIGINT" : "SIGTERM") +
                      ", shutting down");
            server.stop();
        });

        try {
            server.listen();
        } catch (...) {
            pthread_kill(waiter.native_handle(), SIGTERM); // unblock the waiter
            waiter.join();
            throw;
        }
        waiter.join();
        Log::info("Bye");
        return 0;
    } catch (const std::exception& e) {
        Log::error(e.what());
        return 1;
    }
}
