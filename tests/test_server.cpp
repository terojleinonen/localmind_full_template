#include "doctest/doctest.h"

#include <thread>

#include "TestUtil.hpp"
#include "cpp-httplib/httplib.h"
#include "json/json.hpp"
#include "localmind/QueryEngine.hpp"
#include "localmind/Server.hpp"

using namespace localmind;
using nlohmann::json;

namespace {

// Spins up a real server on a random local port for the duration of a test.
struct TestServer {
    explicit TestServer(Config cfg = {}) : store(":memory:"), engine(embedder, store, &llm) {
        cfg.port = 0;
        cfg.webDir = web.path().string();
        std::ofstream(web.file("index.html")) << "<h1>ui</h1>";
        server = std::make_unique<ServerApp>(cfg, engine);
        port = server->bind();
        thread = std::thread([this] { server->listen(); });
        server->waitUntilReady();
    }
    ~TestServer() {
        server->stop();
        thread.join();
    }
    httplib::Client client() const {
        httplib::Client c("127.0.0.1", port);
        c.set_read_timeout(10, 0);
        return c;
    }

    testutil::TempDir web;
    HashingEmbedder embedder;
    VectorStore store;
    testutil::FakeLlm llm;
    QueryEngine engine;
    std::unique_ptr<ServerApp> server;
    std::thread thread;
    int port = 0;
};

json body(const httplib::Result& r) { return json::parse(r->body); }

} // namespace

TEST_CASE("health, stats and static UI") {
    TestServer ts;
    auto c = ts.client();

    auto h = c.Get("/api/health");
    REQUIRE(h);
    CHECK(h->status == 200);
    CHECK(body(h)["status"] == "ok");
    CHECK(body(h)["llm"]["name"] == "fake");
    CHECK(h->get_header_value("X-Content-Type-Options") == "nosniff");

    auto s = c.Get("/api/stats");
    CHECK(body(s)["documents"] == 0);

    auto ui = c.Get("/");
    REQUIRE(ui);
    CHECK(ui->status == 200);
    CHECK(ui->body == "<h1>ui</h1>");

    auto missing = c.Get("/api/nope");
    CHECK(missing->status == 404);
    CHECK(body(missing)["error"] == "not found");
}

TEST_CASE("document lifecycle over HTTP") {
    TestServer ts;
    auto c = ts.client();

    json doc = {{"name", "space.txt"}, {"text", "Olympus Mons on Mars is the tallest volcano."}};
    auto created = c.Post("/api/documents", doc.dump(), "application/json");
    REQUIRE(created);
    CHECK(created->status == 201);
    auto id = body(created)["id"].get<int>();
    CHECK(body(created)["duplicate"] == false);

    auto dup = c.Post("/api/documents", doc.dump(), "application/json");
    CHECK(dup->status == 200);
    CHECK(body(dup)["duplicate"] == true);

    auto list = c.Get("/api/documents");
    CHECK(body(list)["documents"].size() == 1);

    auto one = c.Get("/api/documents/" + std::to_string(id));
    CHECK(one->status == 200);
    CHECK(body(one)["chunk_texts"].size() == 1);

    auto q = c.Post("/api/query", R"({"question":"tallest volcano?","top_k":2})", "application/json");
    REQUIRE(q);
    CHECK(q->status == 200);
    CHECK(body(q)["mode"] == "llm");
    CHECK(body(q)["answer"] == "canned answer [1]");
    CHECK(body(q)["sources"][0]["document_name"] == "space.txt");
    CHECK(body(q)["sources"][0]["n"] == 1);

    auto del = c.Delete("/api/documents/" + std::to_string(id));
    CHECK(del->status == 204);
    CHECK(c.Delete("/api/documents/" + std::to_string(id))->status == 404);
    CHECK(c.Get("/api/documents/" + std::to_string(id))->status == 404);
}

TEST_CASE("request validation") {
    TestServer ts;
    auto c = ts.client();
    auto post = [&](const std::string& path, const std::string& payload) {
        return c.Post(path, payload, "application/json")->status;
    };
    CHECK(post("/api/documents", "not json") == 400);
    CHECK(post("/api/documents", "[]") == 400);
    CHECK(post("/api/documents", R"({"name":"a"})") == 400);
    CHECK(post("/api/documents", R"({"name":"","text":"x"})") == 400);
    CHECK(post("/api/documents", R"({"name":"a","text":"  "})") == 422);
    CHECK(post("/api/query", R"({})") == 400);
    CHECK(post("/api/query", R"({"question":"   "})") == 400);
    CHECK(post("/api/query", R"({"question":"q","top_k":0})") == 400);
    CHECK(post("/api/query", R"({"question":"q","top_k":"3"})") == 400);
    CHECK(c.Delete("/api/documents/abc")->status == 400);
}

TEST_CASE("oversized bodies are rejected") {
    Config cfg;
    cfg.maxBodyBytes = 1024;
    TestServer ts(cfg);
    auto c = ts.client();
    json doc = {{"name", "big"}, {"text", std::string(4096, 'a')}};
    auto r = c.Post("/api/documents", doc.dump(), "application/json");
    REQUIRE(r);
    CHECK(r->status == 413);
}

TEST_CASE("CORS preflight when an origin is configured") {
    Config cfg;
    cfg.corsOrigin = "http://localhost:3000";
    TestServer ts(cfg);
    auto c = ts.client();
    auto r = c.Options("/api/query");
    REQUIRE(r);
    CHECK(r->status == 204);
    CHECK(r->get_header_value("Access-Control-Allow-Origin") == "http://localhost:3000");
}

TEST_CASE("a second server cannot bind the same port") {
    TestServer ts;
    Config cfg;
    cfg.port = ts.port;
    ServerApp other(cfg, ts.engine);
    CHECK_THROWS_AS(other.bind(), std::runtime_error);
}
