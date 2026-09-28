#include "localmind/Embedder.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "OllamaHttp.hpp"
#include "localmind/Config.hpp"
#include "localmind/Log.hpp"
#include "localmind/TextProcessor.hpp"

namespace localmind {

void l2Normalize(std::vector<float>& v) {
    double norm = 0.0;
    for (float x : v) norm += static_cast<double>(x) * x;
    if (norm <= 0.0) return;
    auto inv = static_cast<float>(1.0 / std::sqrt(norm));
    for (float& x : v) x *= inv;
}

std::vector<std::vector<float>>
Embedder::embedBatch(const std::vector<std::string>& texts) const {
    std::vector<std::vector<float>> out;
    out.reserve(texts.size());
    for (const auto& t : texts) out.push_back(embed(t));
    return out;
}

// ---------------------------------------------------------------- hashing

namespace {

std::uint64_t fnv1a(const std::string& s) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

// Very light stemming so "documents" matches "document".
std::string stem(std::string t) {
    auto ends = [&](const char* suf) {
        std::string s(suf);
        return t.size() > s.size() + 2 && t.compare(t.size() - s.size(), s.size(), s) == 0;
    };
    if (ends("ies")) t.replace(t.size() - 3, 3, "y");
    else if (ends("sses")) t.erase(t.size() - 2);
    else if (ends("s") && !ends("ss") && !ends("us") && !ends("is")) t.pop_back();
    return t;
}

} // namespace

HashingEmbedder::HashingEmbedder(std::size_t dim) : dim_(dim) {
    if (dim_ == 0) throw std::invalid_argument("embedding dimension must be > 0");
}

std::string HashingEmbedder::id() const { return "hashing-v1-" + std::to_string(dim_); }

std::vector<float> HashingEmbedder::embed(const std::string& text) const {
    std::unordered_map<std::string, float> features;
    std::string prev;
    for (const auto& raw : TextProcessor::tokenize(text)) {
        if (TextProcessor::isStopword(raw)) {
            prev.clear();
            continue;
        }
        auto tok = stem(raw);
        features["u:" + tok] += 1.0f;
        if (!prev.empty()) features["b:" + prev + " " + tok] += 0.5f;
        prev = tok;
    }

    std::vector<float> vec(dim_, 0.0f);
    for (const auto& [feature, tf] : features) {
        auto h = fnv1a(feature);
        float sign = (h >> 63) ? -1.0f : 1.0f;
        float weight = tf >= 1.0f ? 1.0f + std::log(tf) : tf; // sub-linear tf
        vec[h % dim_] += sign * weight;
    }
    l2Normalize(vec);
    return vec;
}

// ---------------------------------------------------------------- ollama

OllamaEmbedder::OllamaEmbedder(std::string baseUrl, std::string model, int timeoutSec)
    : baseUrl_(std::move(baseUrl)), model_(std::move(model)), timeoutSec_(timeoutSec) {
    dim_ = embed("dimension probe").size();
    if (dim_ == 0) throw std::runtime_error("Ollama returned an empty embedding");
}

std::string OllamaEmbedder::id() const {
    return "ollama-" + model_ + "-" + std::to_string(dim_);
}

std::size_t OllamaEmbedder::dim() const { return dim_; }

std::vector<float> OllamaEmbedder::embed(const std::string& text) const {
    return embedBatch({text}).front();
}

std::vector<std::vector<float>>
OllamaEmbedder::embedBatch(const std::vector<std::string>& texts) const {
    constexpr std::size_t kBatch = 32;
    std::vector<std::vector<float>> out;
    out.reserve(texts.size());

    for (std::size_t i = 0; i < texts.size(); i += kBatch) {
        nlohmann::json input = nlohmann::json::array();
        for (std::size_t j = i; j < std::min(texts.size(), i + kBatch); ++j)
            input.push_back(texts[j]);

        auto res = detail::ollamaPost(baseUrl_, "/api/embed",
                                      {{"model", model_}, {"input", input}}, timeoutSec_);
        if (!res.contains("embeddings") || res["embeddings"].size() != input.size())
            throw std::runtime_error("Ollama /api/embed: unexpected response shape");

        for (auto& e : res["embeddings"]) {
            auto v = e.get<std::vector<float>>();
            if (dim_ != 0 && v.size() != dim_)
                throw std::runtime_error("Ollama returned an embedding of unexpected size");
            l2Normalize(v);
            out.push_back(std::move(v));
        }
    }
    return out;
}

std::unique_ptr<Embedder> makeEmbedder(const Config& cfg) {
    if (cfg.embedder == "ollama") {
        Log::info("Using Ollama embeddings: model=" + cfg.embedModel + " url=" + cfg.ollamaUrl);
        try {
            return std::make_unique<OllamaEmbedder>(cfg.ollamaUrl, cfg.embedModel,
                                                    cfg.ollamaTimeoutSec);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(e.what()) +
                                     "\n  Is Ollama running? Try: ollama pull " + cfg.embedModel);
        }
    }
    Log::info("Using built-in hashing embeddings (offline, lexical)");
    return std::make_unique<HashingEmbedder>();
}

} // namespace localmind
