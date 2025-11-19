#pragma once
#include <string>
#include <vector>
#include "Embedder.hpp"
#include "VectorStore.hpp"

namespace localmind {

class QueryEngine {
public:
    QueryEngine(Embedder& embedder, VectorStore& store);

    void indexFile(const std::string& filePath);
    void indexText(const std::string& text);

    std::vector<SearchResult> query(const std::string& text,
                                    int topK = 5) const;

private:
    Embedder& embedder_;
    VectorStore& store_;
};

} // namespace localmind
