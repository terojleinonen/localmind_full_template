#include "localmind/VectorStore.hpp"
#include "localmind/Utils.hpp"
#include <algorithm>
#include <cmath>

namespace localmind {

VectorStore::VectorStore(const std::string& dbPath, const std::string& indexPath)
    : dbPath_(dbPath), indexPath_(indexPath)
{
    Utils::log("VectorStore (in-memory) created. db=" + dbPath_ + " index=" + indexPath_);
}

void VectorStore::add(const std::vector<float>& embedding, const std::string& snippet) {
    embeddings_.push_back(embedding);
    snippets_.push_back(snippet);
}

std::vector<SearchResult> VectorStore::search(const std::vector<float>& embedding,
                                              int topK) const {
    std::vector<SearchResult> results;
    if (embeddings_.empty()) return results;

    auto cosine = [](const std::vector<float>& a, const std::vector<float>& b) -> float {
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }
        if (na == 0 || nb == 0) return 0.0f;
        return dot / (std::sqrt(na) * std::sqrt(nb));
    };

    for (std::size_t i = 0; i < embeddings_.size(); ++i) {
        float score = cosine(embedding, embeddings_[i]);
        results.push_back({ snippets_[i], score });
    }

    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    if ((int)results.size() > topK) results.resize(topK);
    return results;
}

} // namespace localmind
