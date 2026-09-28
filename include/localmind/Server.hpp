#pragma once
#include <memory>
#include <string>

#include "Config.hpp"

namespace httplib { class Server; }

namespace localmind {

class QueryEngine;

// REST API + static web UI.
//
//   GET    /api/health            liveness + backend info
//   GET    /api/stats             index statistics
//   GET    /api/documents         list documents
//   POST   /api/documents         {name, text} -> index a document
//   GET    /api/documents/:id     document metadata + chunks
//   DELETE /api/documents/:id     remove a document
//   POST   /api/query             {question, top_k?} -> answer + sources
class ServerApp {
public:
    ServerApp(const Config& cfg, QueryEngine& engine);
    ~ServerApp();

    // Binds to cfg.host:cfg.port (port 0 picks a free port). Returns the
    // bound port; throws on failure.
    int bind();
    // Serves requests until stop() is called (blocking).
    void listen();
    void stop();
    void waitUntilReady() const;

private:
    void registerRoutes();

    Config cfg_;
    QueryEngine& engine_;
    std::unique_ptr<httplib::Server> svr_;
};

} // namespace localmind
