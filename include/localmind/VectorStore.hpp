#pragma once
#include <string>
#include <vector>

namespace localmind {

struct SearchResult {
    std::string snippet;
    float score;
};

// In-memory vector store.
// TODO: replace with FAISS + SQLite.
class VectorStore {
public:
    VectorStore(const std::string& dbPath, const std::string& indexPath);

    void add(const std::vector<float>& embedding, const std::string& snippet);
    std::vector<SearchResult> search(const std::vector<float>& embedding,
                                     int topK = 5) const;

private:
    std::string dbPath_;
    std::string indexPath_;
    std::vector<std::vector<float>> embeddings_;
    std::vector<std::string> snippets_;
};

} // namespace localmind
