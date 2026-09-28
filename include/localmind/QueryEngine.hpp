#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Embedder.hpp"
#include "LlmClient.hpp"
#include "VectorStore.hpp"

namespace localmind {

struct IndexResult {
    Document document;
    bool duplicate = false; // identical content was already indexed
};

struct Answer {
    std::string text;
    std::string mode;       // "llm" | "extractive" | "empty"
    std::string warning;    // set when the LLM failed and we fell back
    std::vector<SearchResult> sources;
};

struct EngineOptions {
    std::size_t chunkSize = 800;
    std::size_t chunkOverlap = 120;
    int topK = 4;
    float minScore = 0.05f;
};

// Retrieval-augmented question answering over the vector store.
class QueryEngine {
public:
    // llm may be null: answers are then extracted from the best passages.
    QueryEngine(const Embedder& embedder, VectorStore& store,
                const LlmClient* llm, EngineOptions options = {});

    // Re-embeds stored chunks if they were produced by a different embedder.
    // Returns the number of chunks re-embedded.
    std::size_t syncEmbeddings();

    IndexResult indexText(const std::string& name, const std::string& text);
    IndexResult indexFile(const std::string& path);
    // Indexes .txt / .md / .markdown files under a directory (recursive).
    std::vector<IndexResult> indexPath(const std::string& path);

    std::vector<SearchResult> search(const std::string& question, int topK) const;
    Answer ask(const std::string& question, int topK = 0) const;

    const Embedder& embedder() const { return embedder_; }
    const LlmClient* llm() const { return llm_; }
    VectorStore& store() { return store_; }
    const EngineOptions& options() const { return options_; }

    static std::string buildPrompt(const std::string& question,
                                   const std::vector<SearchResult>& sources);
    static std::string extractiveAnswer(const std::string& question,
                                        const std::vector<SearchResult>& sources);

private:
    const Embedder& embedder_;
    VectorStore& store_;
    const LlmClient* llm_;
    EngineOptions options_;
};

} // namespace localmind
