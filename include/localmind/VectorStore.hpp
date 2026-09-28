#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace localmind {

struct Document {
    std::int64_t id = 0;
    std::string name;
    std::string contentHash;
    std::int64_t chars = 0;
    std::int64_t chunkCount = 0;
    std::string createdAt; // ISO-8601 UTC
};

struct SearchResult {
    std::int64_t chunkId = 0;
    std::int64_t documentId = 0;
    std::string documentName;
    int chunkIndex = 0;
    std::string text;
    float score = 0.0f;
};

struct StoreStats {
    std::size_t documents = 0;
    std::size_t chunks = 0;
    std::int64_t chars = 0;
    std::size_t dim = 0;
    std::string embedderId;
};

// Durable document/chunk/vector store.
//
// SQLite is the source of truth; every vector is also kept in memory for
// brute-force cosine search, which is fast enough for tens of thousands of
// chunks. Thread-safe: searches take a shared lock, writes an exclusive one.
class VectorStore {
public:
    // dbPath may be ":memory:". Parent directories are created as needed.
    explicit VectorStore(const std::string& dbPath);
    ~VectorStore();
    VectorStore(const VectorStore&) = delete;
    VectorStore& operator=(const VectorStore&) = delete;

    // Embedder the stored vectors were produced with ("" if none yet).
    std::string embedderId() const;

    // Inserts a document with its chunks and (normalised) embeddings.
    Document addDocument(const std::string& name, const std::string& contentHash,
                         std::int64_t chars,
                         const std::vector<std::string>& chunks,
                         const std::vector<std::vector<float>>& embeddings,
                         const std::string& embedderId);

    std::optional<Document> findByHash(const std::string& contentHash) const;
    std::optional<Document> getDocument(std::int64_t id) const;
    std::vector<Document> listDocuments() const;
    std::vector<std::string> documentChunks(std::int64_t id) const;
    bool removeDocument(std::int64_t id);

    std::vector<SearchResult> search(const std::vector<float>& query,
                                     int topK, float minScore = 0.0f) const;

    StoreStats stats() const;

    // All chunk ids + texts, for re-embedding after an embedder change.
    std::vector<std::pair<std::int64_t, std::string>> allChunks() const;
    // Replaces every vector at once (ids must match allChunks()).
    void replaceEmbeddings(const std::vector<std::int64_t>& chunkIds,
                           const std::vector<std::vector<float>>& embeddings,
                           const std::string& embedderId);

private:
    struct Entry {
        std::int64_t chunkId;
        std::int64_t documentId;
        int chunkIndex;
        std::string text;
        std::vector<float> vec;
    };

    void exec(const char* sql);
    void migrate();
    void loadAll();
    void setMeta(const std::string& key, const std::string& value);

    sqlite3* db_ = nullptr;
    mutable std::shared_mutex mu_;
    std::vector<Entry> entries_;
    std::unordered_map<std::int64_t, Document> docs_;
    std::string embedderId_;
    std::size_t dim_ = 0;
};

} // namespace localmind
