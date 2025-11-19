#include "localmind/QueryEngine.hpp"
#include "localmind/TextProcessor.hpp"
#include "localmind/Utils.hpp"

namespace localmind {

QueryEngine::QueryEngine(Embedder& embedder, VectorStore& store)
    : embedder_(embedder), store_(store)
{}

void QueryEngine::indexFile(const std::string& filePath) {
    Utils::log("Indexing file: " + filePath);
    auto text = TextProcessor::readFile(filePath);
    indexText(text);
}

void QueryEngine::indexText(const std::string& text) {
    auto chunks = TextProcessor::chunk(text, 512);
    for (const auto& c : chunks) {
        auto emb = embedder_.getEmbedding(c);
        store_.add(emb, c);
    }
}

std::vector<SearchResult> QueryEngine::query(const std::string& text, int topK) const {
    Utils::log("Query: " + text);
    auto emb = embedder_.getEmbedding(text);
    return store_.search(emb, topK);
}

} // namespace localmind
