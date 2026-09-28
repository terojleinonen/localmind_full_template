#include "doctest/doctest.h"

#include <stdexcept>

#include "TestUtil.hpp"
#include "localmind/VectorStore.hpp"

using localmind::VectorStore;

namespace {
std::vector<float> unit(std::size_t dim, std::size_t hot) {
    std::vector<float> v(dim, 0.0f);
    v[hot] = 1.0f;
    return v;
}
} // namespace

TEST_CASE("store adds, searches and removes documents") {
    VectorStore store(":memory:");
    auto a = store.addDocument("a.txt", "h1", 10, {"alpha", "beta"}, {unit(4, 0), unit(4, 1)}, "emb");
    auto b = store.addDocument("b.txt", "h2", 5, {"gamma"}, {unit(4, 2)}, "emb");
    CHECK(a.chunkCount == 2);
    CHECK_FALSE(a.createdAt.empty());

    auto hits = store.search(unit(4, 1), 5);
    REQUIRE(hits.size() == 1); // zero-score chunks are filtered out
    CHECK(hits[0].text == "beta");
    CHECK(hits[0].documentName == "a.txt");
    CHECK(hits[0].chunkIndex == 1);
    CHECK(hits[0].score == doctest::Approx(1.0f));

    auto s = store.stats();
    CHECK(s.documents == 2);
    CHECK(s.chunks == 3);
    CHECK(s.chars == 15);
    CHECK(s.dim == 4);

    CHECK(store.listDocuments().front().id == b.id); // newest first
    CHECK(store.documentChunks(a.id) == std::vector<std::string>{"alpha", "beta"});
    CHECK(store.findByHash("h2")->id == b.id);

    CHECK(store.removeDocument(a.id));
    CHECK_FALSE(store.removeDocument(a.id));
    CHECK(store.search(unit(4, 1), 5).empty());
    CHECK(store.stats().chunks == 1);
}

TEST_CASE("search honours topK and minScore ordering") {
    VectorStore store(":memory:");
    std::vector<std::vector<float>> vecs = {{1, 0}, {0.8f, 0.6f}, {0.6f, 0.8f}, {0, 1}};
    store.addDocument("d", "h", 1, {"c0", "c1", "c2", "c3"}, vecs, "emb");
    auto hits = store.search({1, 0}, 2);
    REQUIRE(hits.size() == 2);
    CHECK(hits[0].text == "c0");
    CHECK(hits[1].text == "c1");
    CHECK(store.search({1, 0}, 10, 0.7f).size() == 2);
}

TEST_CASE("store persists across reopen") {
    testutil::TempDir dir;
    auto db = dir.file("nested/dir/test.db");
    std::int64_t id;
    {
        VectorStore store(db);
        id = store.addDocument("notes.md", "hash", 3, {"abc"}, {unit(3, 0)}, "emb-x").id;
    }
    VectorStore store(db);
    CHECK(store.embedderId() == "emb-x");
    REQUIRE(store.getDocument(id));
    CHECK(store.getDocument(id)->name == "notes.md");
    CHECK(store.getDocument(id)->chunkCount == 1);
    CHECK(store.search(unit(3, 0), 1).at(0).text == "abc");
}

TEST_CASE("store rejects duplicate content and mixed embedders") {
    VectorStore store(":memory:");
    store.addDocument("a", "same", 1, {"x"}, {unit(2, 0)}, "emb-1");
    CHECK_THROWS_AS(store.addDocument("b", "same", 1, {"y"}, {unit(2, 0)}, "emb-1"),
                    std::runtime_error);
    CHECK(store.stats().documents == 1); // failed insert rolled back
    CHECK_THROWS_AS(store.addDocument("c", "other", 1, {"z"}, {unit(2, 0)}, "emb-2"),
                    std::logic_error);
}

TEST_CASE("replaceEmbeddings swaps vectors and embedder id") {
    VectorStore store(":memory:");
    store.addDocument("a", "h", 1, {"x", "y"}, {unit(2, 0), unit(2, 1)}, "old");
    auto chunks = store.allChunks();
    REQUIRE(chunks.size() == 2);
    store.replaceEmbeddings({chunks[0].first, chunks[1].first}, {unit(3, 2), unit(3, 1)}, "new");
    CHECK(store.embedderId() == "new");
    CHECK(store.stats().dim == 3);
    CHECK(store.search(unit(3, 2), 1).at(0).text == "x");
}
