#include "localmind/Server.hpp"
#include "localmind/Embedder.hpp"
#include "localmind/VectorStore.hpp"
#include "localmind/QueryEngine.hpp"
#include "localmind/Utils.hpp"

#include "cpp-httplib/httplib.h"
#include "json/json.hpp"

namespace localmind {

using nlohmann::json;

ServerApp::ServerApp(const std::string& host, int port)
    : host_(host), port_(port)
{}

void ServerApp::run() {
    Utils::log("Starting HTTP server on " + host_ + ":" + std::to_string(port_));

    Embedder embedder("data/models/dummy.onnx");
    VectorStore store("data/db/localmind.db", "data/db/index.faiss");
    QueryEngine engine(embedder, store);

    httplib::Server svr;

    svr.Post("/index", [&](const httplib::Request& req, httplib::Response& res) {
        // TODO: parse JSON, index text, return result
        json body = json::parse(req.body);
        // Placeholder: no real JSON yet
        (void)body;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content("{"status":"ok","note":"index endpoint stub"}", "application/json");
    });

    svr.Post("/query", [&](const httplib::Request& req, httplib::Response& res) {
        // TODO: parse JSON, run engine.query, return best hits
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content("{"answer":"query endpoint stub (wire to QueryEngine)"}", "application/json");
    });

    svr.listen(host_.c_str(), port_);
}

} // namespace localmind
