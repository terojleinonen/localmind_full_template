#include "localmind/VectorStore.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include "localmind/Log.hpp"

namespace localmind {

namespace {

// RAII wrapper for a prepared statement.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
            throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db));
    }
    ~Stmt() { sqlite3_finalize(stmt_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    Stmt& bind(int i, std::int64_t v) { check(sqlite3_bind_int64(stmt_, i, v)); return *this; }
    Stmt& bind(int i, const std::string& v) {
        check(sqlite3_bind_text(stmt_, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT));
        return *this;
    }
    Stmt& bind(int i, const std::vector<float>& v) {
        check(sqlite3_bind_blob(stmt_, i, v.data(), static_cast<int>(v.size() * sizeof(float)),
                                SQLITE_TRANSIENT));
        return *this;
    }

    // Returns true while rows are available.
    bool step() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("sqlite step: ") + sqlite3_errmsg(db_));
    }
    void reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }

    std::int64_t i64(int col) const { return sqlite3_column_int64(stmt_, col); }
    std::string str(int col) const {
        auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
        return p ? std::string(p, static_cast<std::size_t>(sqlite3_column_bytes(stmt_, col)))
                 : std::string();
    }
    std::vector<float> floats(int col) const {
        auto bytes = static_cast<std::size_t>(sqlite3_column_bytes(stmt_, col));
        std::vector<float> v(bytes / sizeof(float));
        if (bytes) std::memcpy(v.data(), sqlite3_column_blob(stmt_, col), v.size() * sizeof(float));
        return v;
    }

private:
    void check(int rc) {
        if (rc != SQLITE_OK)
            throw std::runtime_error(std::string("sqlite bind: ") + sqlite3_errmsg(db_));
    }
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

// Rolls back unless commit() is called.
class Transaction {
public:
    explicit Transaction(sqlite3* db) : db_(db) { run("BEGIN IMMEDIATE"); }
    ~Transaction() { if (!done_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
    void commit() { run("COMMIT"); done_ = true; }

private:
    void run(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown";
            sqlite3_free(err);
            throw std::runtime_error(std::string("sqlite ") + sql + ": " + msg);
        }
    }
    sqlite3* db_;
    bool done_ = false;
};

float dot(const std::vector<float>& a, const std::vector<float>& b) {
    float s = 0.0f;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

} // namespace

VectorStore::VectorStore(const std::string& dbPath) {
    if (dbPath != ":memory:") {
        auto parent = std::filesystem::path(dbPath).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    }
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw std::runtime_error("cannot open database " + dbPath + ": " + msg);
    }
    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA foreign_keys = ON");
    if (dbPath != ":memory:") exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
    migrate();
    loadAll();
    Log::info("Opened " + dbPath + ": " + std::to_string(docs_.size()) + " documents, " +
              std::to_string(entries_.size()) + " chunks");
}

VectorStore::~VectorStore() { sqlite3_close(db_); }

void VectorStore::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error(std::string("sqlite: ") + msg);
    }
}

void VectorStore::migrate() {
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS meta (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS documents (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT    NOT NULL,
            content_hash TEXT    NOT NULL UNIQUE,
            chars        INTEGER NOT NULL,
            created_at   TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        );
        CREATE TABLE IF NOT EXISTS chunks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            document_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
            chunk_index INTEGER NOT NULL,
            text        TEXT    NOT NULL,
            embedding   BLOB    NOT NULL
        );
        CREATE INDEX IF NOT EXISTS chunks_document ON chunks(document_id);
        INSERT OR IGNORE INTO meta(key, value) VALUES ('schema_version', '1');
    )sql");
}

void VectorStore::setMeta(const std::string& key, const std::string& value) {
    Stmt(db_, "INSERT INTO meta(key, value) VALUES (?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value")
        .bind(1, key).bind(2, value).step();
}

void VectorStore::loadAll() {
    {
        Stmt s(db_, "SELECT value FROM meta WHERE key = 'embedder'");
        if (s.step()) embedderId_ = s.str(0);
    }
    {
        Stmt s(db_, "SELECT d.id, d.name, d.content_hash, d.chars, d.created_at, "
                    "(SELECT COUNT(*) FROM chunks c WHERE c.document_id = d.id) "
                    "FROM documents d");
        while (s.step()) {
            Document d{s.i64(0), s.str(1), s.str(2), s.i64(3), s.i64(5), s.str(4)};
            docs_.emplace(d.id, std::move(d));
        }
    }
    Stmt s(db_, "SELECT id, document_id, chunk_index, text, embedding FROM chunks "
                "ORDER BY document_id, chunk_index");
    while (s.step()) {
        Entry e{s.i64(0), s.i64(1), static_cast<int>(s.i64(2)), s.str(3), s.floats(4)};
        if (dim_ == 0) dim_ = e.vec.size();
        entries_.push_back(std::move(e));
    }
}

std::string VectorStore::embedderId() const {
    std::shared_lock lock(mu_);
    return embedderId_;
}

Document VectorStore::addDocument(const std::string& name, const std::string& contentHash,
                                  std::int64_t chars,
                                  const std::vector<std::string>& chunks,
                                  const std::vector<std::vector<float>>& embeddings,
                                  const std::string& embedderId) {
    if (chunks.size() != embeddings.size())
        throw std::invalid_argument("chunks and embeddings differ in size");

    std::unique_lock lock(mu_);
    if (!embedderId_.empty() && embedderId_ != embedderId && !entries_.empty())
        throw std::logic_error("store holds vectors from '" + embedderId_ +
                               "', refusing to mix with '" + embedderId + "'");

    Transaction tx(db_);
    Stmt(db_, "INSERT INTO documents(name, content_hash, chars) VALUES (?, ?, ?)")
        .bind(1, name).bind(2, contentHash).bind(3, chars).step();
    Document doc{sqlite3_last_insert_rowid(db_), name, contentHash, chars,
                 static_cast<std::int64_t>(chunks.size()), {}};
    {
        Stmt s(db_, "SELECT created_at FROM documents WHERE id = ?");
        s.bind(1, doc.id);
        if (s.step()) doc.createdAt = s.str(0);
    }

    std::vector<Entry> added;
    Stmt insChunk(db_, "INSERT INTO chunks(document_id, chunk_index, text, embedding) "
                       "VALUES (?, ?, ?, ?)");
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        insChunk.bind(1, doc.id).bind(2, static_cast<std::int64_t>(i))
            .bind(3, chunks[i]).bind(4, embeddings[i]);
        insChunk.step();
        insChunk.reset();
        added.push_back({sqlite3_last_insert_rowid(db_), doc.id, static_cast<int>(i),
                         chunks[i], embeddings[i]});
    }
    if (embedderId_ != embedderId) setMeta("embedder", embedderId);
    tx.commit();

    // Only touch in-memory state once the transaction has committed.
    embedderId_ = embedderId;
    if (!added.empty()) dim_ = added.front().vec.size();
    for (auto& e : added) entries_.push_back(std::move(e));
    docs_.emplace(doc.id, doc);
    return doc;
}

std::optional<Document> VectorStore::findByHash(const std::string& contentHash) const {
    std::shared_lock lock(mu_);
    for (const auto& [id, d] : docs_)
        if (d.contentHash == contentHash) return d;
    return std::nullopt;
}

std::optional<Document> VectorStore::getDocument(std::int64_t id) const {
    std::shared_lock lock(mu_);
    auto it = docs_.find(id);
    if (it == docs_.end()) return std::nullopt;
    return it->second;
}

std::vector<Document> VectorStore::listDocuments() const {
    std::shared_lock lock(mu_);
    std::vector<Document> out;
    out.reserve(docs_.size());
    for (const auto& [id, d] : docs_) out.push_back(d);
    std::sort(out.begin(), out.end(),
              [](const Document& a, const Document& b) { return a.id > b.id; });
    return out;
}

std::vector<std::string> VectorStore::documentChunks(std::int64_t id) const {
    std::shared_lock lock(mu_);
    std::vector<std::string> out;
    for (const auto& e : entries_)
        if (e.documentId == id) out.push_back(e.text);
    return out;
}

bool VectorStore::removeDocument(std::int64_t id) {
    std::unique_lock lock(mu_);
    if (!docs_.count(id)) return false;
    Stmt(db_, "DELETE FROM documents WHERE id = ?").bind(1, id).step();
    docs_.erase(id);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [id](const Entry& e) { return e.documentId == id; }),
                   entries_.end());
    return true;
}

std::vector<SearchResult> VectorStore::search(const std::vector<float>& query,
                                              int topK, float minScore) const {
    std::shared_lock lock(mu_);
    std::vector<std::pair<float, const Entry*>> scored;
    scored.reserve(entries_.size());
    for (const auto& e : entries_) {
        float s = dot(query, e.vec); // vectors are L2-normalised: dot == cosine
        if (s >= minScore && s > 0.0f) scored.emplace_back(s, &e);
    }

    const auto k = std::min<std::size_t>(scored.size(), static_cast<std::size_t>(std::max(topK, 0)));
    std::partial_sort(scored.begin(), scored.begin() + static_cast<std::ptrdiff_t>(k), scored.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<SearchResult> out;
    out.reserve(k);
    for (std::size_t i = 0; i < k; ++i) {
        const Entry& e = *scored[i].second;
        auto doc = docs_.find(e.documentId);
        out.push_back({e.chunkId, e.documentId,
                       doc != docs_.end() ? doc->second.name : std::string(),
                       e.chunkIndex, e.text, scored[i].first});
    }
    return out;
}

StoreStats VectorStore::stats() const {
    std::shared_lock lock(mu_);
    StoreStats s;
    s.documents = docs_.size();
    s.chunks = entries_.size();
    for (const auto& [id, d] : docs_) s.chars += d.chars;
    s.dim = dim_;
    s.embedderId = embedderId_;
    return s;
}

std::vector<std::pair<std::int64_t, std::string>> VectorStore::allChunks() const {
    std::shared_lock lock(mu_);
    std::vector<std::pair<std::int64_t, std::string>> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.emplace_back(e.chunkId, e.text);
    return out;
}

void VectorStore::replaceEmbeddings(const std::vector<std::int64_t>& chunkIds,
                                    const std::vector<std::vector<float>>& embeddings,
                                    const std::string& embedderId) {
    if (chunkIds.size() != embeddings.size())
        throw std::invalid_argument("chunk ids and embeddings differ in size");

    std::unique_lock lock(mu_);
    Transaction tx(db_);
    Stmt upd(db_, "UPDATE chunks SET embedding = ? WHERE id = ?");
    for (std::size_t i = 0; i < chunkIds.size(); ++i) {
        upd.bind(1, embeddings[i]).bind(2, chunkIds[i]);
        upd.step();
        upd.reset();
    }
    setMeta("embedder", embedderId);
    tx.commit();

    std::unordered_map<std::int64_t, std::size_t> pos;
    for (std::size_t i = 0; i < chunkIds.size(); ++i) pos[chunkIds[i]] = i;
    for (auto& e : entries_) {
        auto it = pos.find(e.chunkId);
        if (it != pos.end()) e.vec = embeddings[it->second];
    }
    embedderId_ = embedderId;
    dim_ = embeddings.empty() ? dim_ : embeddings.front().size();
}

} // namespace localmind
