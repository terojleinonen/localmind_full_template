#pragma once
#include <memory>
#include <string>
#include <vector>

namespace localmind {

struct Config;

// Turns text into a fixed-size, L2-normalised vector.
class Embedder {
public:
    virtual ~Embedder() = default;

    // Stable identifier stored alongside vectors, e.g. "hashing-v1-512".
    // When it changes, existing vectors are re-embedded.
    virtual std::string id() const = 0;
    virtual std::size_t dim() const = 0;
    virtual std::vector<float> embed(const std::string& text) const = 0;

    // Default implementation embeds one by one.
    virtual std::vector<std::vector<float>>
    embedBatch(const std::vector<std::string>& texts) const;
};

// Dependency-free lexical embedder: feature-hashed unigrams + bigrams with
// sub-linear term frequency. Deterministic and fast; good at keyword overlap,
// not at synonyms. Useful offline and as a fallback.
class HashingEmbedder : public Embedder {
public:
    explicit HashingEmbedder(std::size_t dim = 512);
    std::string id() const override;
    std::size_t dim() const override { return dim_; }
    std::vector<float> embed(const std::string& text) const override;

private:
    std::size_t dim_;
};

// Semantic embeddings via Ollama's /api/embed endpoint.
class OllamaEmbedder : public Embedder {
public:
    OllamaEmbedder(std::string baseUrl, std::string model, int timeoutSec);
    std::string id() const override;
    std::size_t dim() const override;
    std::vector<float> embed(const std::string& text) const override;
    std::vector<std::vector<float>>
    embedBatch(const std::vector<std::string>& texts) const override;

private:
    std::string baseUrl_;
    std::string model_;
    int timeoutSec_;
    std::size_t dim_ = 0; // discovered with a probe request in the constructor
};

void l2Normalize(std::vector<float>& v);

// Builds the embedder selected in the config. Throws on failure.
std::unique_ptr<Embedder> makeEmbedder(const Config& cfg);

} // namespace localmind
