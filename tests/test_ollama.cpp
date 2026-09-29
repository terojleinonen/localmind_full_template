#include "doctest/doctest.h"

#include <atomic>
#include <thread>

#include "cpp-httplib/httplib.h"
#include "json/json.hpp"
#include "localmind/Embedder.hpp"
#include "localmind/LlmClient.hpp"

using namespace localmind;
using nlohmann::json;

namespace {

// Minimal stand-in for the Ollama REST API (/api/tags, /api/embed, /api/chat).
struct FakeOllama {
    FakeOllama() {
        svr.Get("/api/tags", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"models":[{"name":"llama3.2:latest"},{"name":"embed:latest"}]})",
                            "application/json");
        });
        svr.Post("/api/embed", [this](const httplib::Request& req, httplib::Response& res) {
            ++embedCalls;
            auto body = json::parse(req.body);
            if (body["model"] != "embed") {
                res.status = 404;
                res.set_content(R"({"error":"model \"x\" not found, try pulling it first"})",
                                "application/json");
                return;
            }
            json out = {{"embeddings", json::array()}};
            for (const auto& text : body["input"]) {
                auto len = static_cast<float>(text.get<std::string>().size());
                out["embeddings"].push_back({len, 1.0f, 0.0f});
            }
            res.set_content(out.dump(), "application/json");
        });
        svr.Post("/api/chat", [this](const httplib::Request& req, httplib::Response& res) {
            lastChat = json::parse(req.body);
            if (lastChat["model"] == "slow") std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            // Like Ollama: done_reason is "length" when num_predict cut the answer short.
            json out = {{"message", {{"role", "assistant"}, {"content", "Mars is red [1]."}}},
                        {"done_reason", lastChat["model"] == "wordy" ? "length" : "stop"}};
            res.set_content(out.dump(), "application/json");
        });
        port = svr.bind_to_any_port("127.0.0.1");
        thread = std::thread([this] { svr.listen_after_bind(); });
        svr.wait_until_ready();
    }
    ~FakeOllama() {
        svr.stop();
        thread.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port); }

    httplib::Server svr;
    std::thread thread;
    int port = 0;
    std::atomic<int> embedCalls{0};
    json lastChat;
};

} // namespace

TEST_CASE("OllamaEmbedder probes dimension, batches and normalises") {
    FakeOllama ollama;
    OllamaEmbedder e(ollama.url(), "embed", 5);
    CHECK(e.dim() == 3);
    CHECK(e.id() == "ollama-embed-3");

    std::vector<std::string> texts(70, "abc");
    int before = ollama.embedCalls;
    auto vecs = e.embedBatch(texts);
    CHECK(vecs.size() == 70);
    CHECK(ollama.embedCalls - before == 3); // batches of 32

    float norm = 0;
    for (float x : vecs[0]) norm += x * x;
    CHECK(norm == doctest::Approx(1.0f));
}

TEST_CASE("OllamaEmbedder surfaces API errors") {
    FakeOllama ollama;
    CHECK_THROWS_WITH_AS(OllamaEmbedder(ollama.url(), "missing", 5),
                         doctest::Contains("not found"), std::runtime_error);
    CHECK_THROWS_WITH_AS(OllamaEmbedder("http://127.0.0.1:1", "embed", 1),
                         doctest::Contains("unreachable"), std::runtime_error);
}

TEST_CASE("OllamaClient checks availability and generates") {
    FakeOllama ollama;
    OllamaClient llm(ollama.url(), "llama3.2", 5);
    CHECK(llm.name() == "ollama:llama3.2");
    CHECK(llm.available());
    CHECK_FALSE(OllamaClient(ollama.url(), "mistral", 5).available());
    CHECK_FALSE(OllamaClient("http://127.0.0.1:1", "llama3.2", 1).available());

    CHECK(llm.generate("sys", "prompt") == "Mars is red [1].");
    CHECK(ollama.lastChat["stream"] == false);
    CHECK(ollama.lastChat["messages"][0]["role"] == "system");
    CHECK(ollama.lastChat["messages"][1]["content"] == "prompt");
}

TEST_CASE("OllamaClient reports a slow model as a timeout, not as unreachable") {
    FakeOllama ollama;
    OllamaClient slow(ollama.url(), "slow", 1);
    CHECK_THROWS_WITH_AS(slow.generate("sys", "prompt"), doctest::Contains("did not respond within 1s"),
                         std::runtime_error);
}

TEST_CASE("OllamaClient caps answer length and flags truncated answers") {
    FakeOllama ollama;
    OllamaClient llm(ollama.url(), "llama3.2", 5, 123);
    CHECK(llm.generate("sys", "prompt") == "Mars is red [1].");
    CHECK(ollama.lastChat["options"]["num_predict"] == 123);

    OllamaClient wordy(ollama.url(), "wordy", 5, 16);
    CHECK(wordy.generate("sys", "prompt").find("cut off at the length limit") != std::string::npos);
}
