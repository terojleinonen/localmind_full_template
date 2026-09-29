#include "localmind/LlmClient.hpp"

#include <stdexcept>

#include "OllamaHttp.hpp"
#include "localmind/Config.hpp"
#include "localmind/Log.hpp"

namespace localmind {

OllamaClient::OllamaClient(std::string baseUrl, std::string model, int timeoutSec,
                           int maxTokens)
    : baseUrl_(std::move(baseUrl)), model_(std::move(model)), timeoutSec_(timeoutSec),
      maxTokens_(maxTokens) {}

bool OllamaClient::available() const {
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(2, 0);
    auto res = cli.Get("/api/tags");
    if (!res || res->status != 200) return false;

    auto tags = nlohmann::json::parse(res->body, nullptr, false);
    if (tags.is_discarded() || !tags.contains("models")) return false;
    // Ollama names models "name:tag"; accept "llama3.2" for "llama3.2:latest".
    for (const auto& m : tags["models"]) {
        auto name = m.value("name", "");
        if (name == model_ || name == model_ + ":latest") return true;
    }
    return false;
}

std::string OllamaClient::generate(const std::string& system,
                                   const std::string& prompt) const {
    nlohmann::json body = {
        {"model", model_},
        {"stream", false},
        {"messages", {{{"role", "system"}, {"content", system}},
                      {{"role", "user"}, {"content", prompt}}}},
        {"options", {{"temperature", 0.2}, {"num_predict", maxTokens_}}},
    };
    auto res = detail::ollamaPost(baseUrl_, "/api/chat", body, timeoutSec_);
    std::string content;
    if (res.contains("message") && res["message"].is_object())
        content = res["message"].value("content", "");
    if (content.empty()) throw std::runtime_error("Ollama returned an empty answer");
    if (res.value("done_reason", "") == "length")
        content += " … (answer cut off at the length limit)";
    return content;
}

std::unique_ptr<LlmClient> makeLlmClient(const Config& cfg) {
    if (cfg.llm == "none") {
        Log::info("LLM disabled: answers are extracted from retrieved passages");
        return nullptr;
    }
    auto client = std::make_unique<OllamaClient>(cfg.ollamaUrl, cfg.chatModel,
                                                 cfg.ollamaTimeoutSec, cfg.maxAnswerTokens);
    if (client->available()) {
        Log::info("Using Ollama chat model " + cfg.chatModel + " at " + cfg.ollamaUrl);
    } else {
        Log::warn("Ollama model '" + cfg.chatModel + "' not reachable at " + cfg.ollamaUrl +
                  " - answers fall back to extractive mode until it is (ollama pull " +
                  cfg.chatModel + ")");
    }
    return client;
}

} // namespace localmind
