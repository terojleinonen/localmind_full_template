#pragma once
#include <memory>
#include <string>

namespace localmind {

struct Config;

// Text generation backend used to phrase answers from retrieved context.
class LlmClient {
public:
    virtual ~LlmClient() = default;
    virtual std::string name() const = 0;
    // Cheap reachability check (used by /api/health).
    virtual bool available() const = 0;
    // Throws std::runtime_error on failure.
    virtual std::string generate(const std::string& system,
                                 const std::string& prompt) const = 0;
};

// Chat completion via Ollama's /api/chat endpoint.
class OllamaClient : public LlmClient {
public:
    OllamaClient(std::string baseUrl, std::string model, int timeoutSec);
    std::string name() const override { return "ollama:" + model_; }
    bool available() const override;
    std::string generate(const std::string& system,
                         const std::string& prompt) const override;

private:
    std::string baseUrl_;
    std::string model_;
    int timeoutSec_;
};

// Returns nullptr when generation is disabled (cfg.llm == "none").
std::unique_ptr<LlmClient> makeLlmClient(const Config& cfg);

} // namespace localmind
