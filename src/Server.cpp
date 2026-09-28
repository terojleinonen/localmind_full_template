#include "localmind/Server.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>

#include "cpp-httplib/httplib.h"
#include "json/json.hpp"
#include "localmind/Log.hpp"
#include "localmind/QueryEngine.hpp"

namespace localmind {

using nlohmann::json;

namespace {

constexpr std::size_t kMaxNameLength = 255;
constexpr std::size_t kMaxQuestionLength = 4000;
constexpr int kMaxTopK = 20;

void sendJson(httplib::Response& res, int status, const json& body) {
    res.status = status;
    // Replace invalid UTF-8 (e.g. odd file names) instead of throwing.
    res.set_content(body.dump(-1, ' ', false, json::error_handler_t::replace),
                    "application/json");
}

void sendError(httplib::Response& res, int status, const std::string& message) {
    sendJson(res, status, {{"error", message}});
}

// Parses the request body as a JSON object; sends 400 and returns false if not.
bool parseBody(const httplib::Request& req, httplib::Response& res, json& out) {
    out = json::parse(req.body, nullptr, false);
    if (out.is_discarded() || !out.is_object()) {
        sendError(res, 400, "request body must be a JSON object");
        return false;
    }
    return true;
}

bool parseId(const httplib::Request& req, std::int64_t& id) {
    auto it = req.path_params.find("id");
    if (it == req.path_params.end()) return false;
    try {
        std::size_t pos = 0;
        id = std::stoll(it->second, &pos);
        return pos == it->second.size() && id > 0;
    } catch (const std::exception&) {
        return false;
    }
}

json toJson(const Document& d) {
    return {{"id", d.id},
            {"name", d.name},
            {"chars", d.chars},
            {"chunks", d.chunkCount},
            {"created_at", d.createdAt}};
}

json toJson(const SearchResult& r, std::size_t n) {
    return {{"n", n},
            {"document_id", r.documentId},
            {"document_name", r.documentName},
            {"chunk_index", r.chunkIndex},
            {"score", r.score},
            {"text", r.text}};
}

} // namespace

ServerApp::ServerApp(const Config& cfg, QueryEngine& engine)
    : cfg_(cfg), engine_(engine), svr_(std::make_unique<httplib::Server>()) {
    auto threads = static_cast<std::size_t>(cfg_.threads);
    svr_->new_task_queue = [threads] { return new httplib::ThreadPool(threads); };
    // httplib defaults to SO_REUSEPORT, which lets a second instance silently
    // share the port. SO_REUSEADDR still allows fast restarts.
    svr_->set_socket_options([](socket_t sock) {
        httplib::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
    });
    svr_->set_payload_max_length(cfg_.maxBodyBytes);
    svr_->set_read_timeout(30, 0);
    svr_->set_write_timeout(30, 0);
    registerRoutes();
}

ServerApp::~ServerApp() = default;

void ServerApp::registerRoutes() {
    auto& svr = *svr_;

    // --- cross-cutting concerns -------------------------------------------
    const std::string cors = cfg_.corsOrigin;
    svr.set_pre_routing_handler([cors](const httplib::Request& req, httplib::Response& res) {
        if (!cors.empty()) {
            res.set_header("Access-Control-Allow-Origin", cors);
            res.set_header("Vary", "Origin");
            if (req.method == "OPTIONS") {
                res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
                res.set_header("Access-Control-Max-Age", "600");
                res.status = 204;
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("Referrer-Policy", "no-referrer");
        res.set_header("X-Frame-Options", "DENY");
        // API responses are never cached; UI assets are revalidated on each load
        // so upgrades show up without a hard refresh.
        res.set_header("Cache-Control",
                       req.path.rfind("/api/", 0) == 0 ? "no-store" : "no-cache");
    });

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (!res.body.empty()) return;
        sendError(res, res.status, res.status == 404 ? "not found"
                                   : res.status == 413 ? "request body too large"
                                                       : httplib::status_message(res.status));
    });

    svr.set_exception_handler(
        [](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
            std::string what = "unknown error";
            try {
                if (ep) std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                what = e.what();
            } catch (...) {
            }
            Log::error(req.method + " " + req.path + " failed: " + what);
            sendError(res, 500, "internal server error");
        });

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::string line = req.method + " " + req.path + " " + std::to_string(res.status) +
                           " " + std::to_string(res.body.size()) + "B " + req.remote_addr;
        if (req.path.rfind("/api/", 0) == 0) Log::info(line);
        else Log::debug(line);
    });

    // --- API ----------------------------------------------------------------
    svr.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        const LlmClient* llm = engine_.llm();
        sendJson(res, 200,
                 {{"status", "ok"},
                  {"version", LOCALMIND_VERSION},
                  {"embedder", engine_.embedder().id()},
                  {"llm",
                   {{"enabled", llm != nullptr},
                    {"name", llm ? llm->name() : "none"},
                    {"available", llm ? llm->available() : false}}}});
    });

    svr.Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
        auto s = engine_.store().stats();
        const auto& o = engine_.options();
        sendJson(res, 200,
                 {{"documents", s.documents},
                  {"chunks", s.chunks},
                  {"chars", s.chars},
                  {"dimensions", s.dim},
                  {"embedder", engine_.embedder().id()},
                  {"chunk_size", o.chunkSize},
                  {"chunk_overlap", o.chunkOverlap},
                  {"top_k", o.topK},
                  {"min_score", o.minScore}});
    });

    svr.Get("/api/documents", [this](const httplib::Request&, httplib::Response& res) {
        json docs = json::array();
        for (const auto& d : engine_.store().listDocuments()) docs.push_back(toJson(d));
        sendJson(res, 200, {{"documents", docs}});
    });

    svr.Get("/api/documents/:id", [this](const httplib::Request& req, httplib::Response& res) {
        std::int64_t id = 0;
        if (!parseId(req, id)) return sendError(res, 400, "invalid document id");
        auto doc = engine_.store().getDocument(id);
        if (!doc) return sendError(res, 404, "document not found");
        auto body = toJson(*doc);
        body["chunk_texts"] = engine_.store().documentChunks(id);
        sendJson(res, 200, body);
    });

    svr.Post("/api/documents", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("name") || !body["name"].is_string() ||
            !body.contains("text") || !body["text"].is_string())
            return sendError(res, 400, "'name' and 'text' must be strings");

        auto name = body["name"].get<std::string>();
        if (name.empty() || name.size() > kMaxNameLength)
            return sendError(res, 400, "'name' must be 1-255 characters");

        try {
            auto result = engine_.indexText(name, body["text"].get<std::string>());
            auto out = toJson(result.document);
            out["duplicate"] = result.duplicate;
            sendJson(res, result.duplicate ? 200 : 201, out);
        } catch (const std::invalid_argument& e) {
            sendError(res, 422, e.what());
        } catch (const std::exception& e) {
            Log::error(std::string("indexing failed: ") + e.what());
            sendError(res, 502, std::string("indexing failed: ") + e.what());
        }
    });

    svr.Delete("/api/documents/:id", [this](const httplib::Request& req, httplib::Response& res) {
        std::int64_t id = 0;
        if (!parseId(req, id)) return sendError(res, 400, "invalid document id");
        if (!engine_.store().removeDocument(id)) return sendError(res, 404, "document not found");
        Log::info("Removed document " + std::to_string(id));
        res.status = 204;
    });

    svr.Post("/api/query", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("question") || !body["question"].is_string())
            return sendError(res, 400, "'question' must be a string");

        auto question = body["question"].get<std::string>();
        if (question.find_first_not_of(" \t\r\n") == std::string::npos)
            return sendError(res, 400, "'question' must not be empty");
        if (question.size() > kMaxQuestionLength)
            return sendError(res, 400, "'question' is too long");

        int topK = 0;
        if (body.contains("top_k")) {
            if (!body["top_k"].is_number_integer())
                return sendError(res, 400, "'top_k' must be an integer");
            topK = body["top_k"].get<int>();
            if (topK < 1 || topK > kMaxTopK)
                return sendError(res, 400, "'top_k' must be between 1 and 20");
        }

        auto started = std::chrono::steady_clock::now();
        auto answer = engine_.ask(question, topK);
        auto tookMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - started).count();

        json sources = json::array();
        for (std::size_t i = 0; i < answer.sources.size(); ++i)
            sources.push_back(toJson(answer.sources[i], i + 1));
        json out = {{"answer", answer.text},
                    {"mode", answer.mode},
                    {"sources", sources},
                    {"took_ms", tookMs}};
        if (!answer.warning.empty()) out["warning"] = answer.warning;
        sendJson(res, 200, out);
    });

    // Unknown /api routes get a JSON 404 rather than falling through to files.
    svr.Get(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        sendError(res, 404, "not found");
    });

    // --- static web UI --------------------------------------------------------
    if (!cfg_.webDir.empty()) {
        if (svr.set_mount_point("/", cfg_.webDir))
            Log::info("Serving web UI from " + std::filesystem::absolute(cfg_.webDir).string());
        else
            Log::warn("Web UI directory '" + cfg_.webDir + "' not found; UI disabled");
    }
}

int ServerApp::bind() {
    int port = cfg_.port;
    bool ok = port == 0 ? (port = svr_->bind_to_any_port(cfg_.host)) > 0
                        : svr_->bind_to_port(cfg_.host, port);
    if (!ok)
        throw std::runtime_error("cannot bind " + cfg_.host + ":" + std::to_string(cfg_.port) +
                                 " (address in use?)");
    cfg_.port = port;
    return port;
}

void ServerApp::listen() {
    if (!svr_->listen_after_bind())
        throw std::runtime_error("HTTP server stopped unexpectedly");
}

void ServerApp::stop() { svr_->stop(); }

void ServerApp::waitUntilReady() const { svr_->wait_until_ready(); }

} // namespace localmind
