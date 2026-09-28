#include "doctest/doctest.h"

#include <fstream>

#include "TestUtil.hpp"
#include "localmind/QueryEngine.hpp"

using namespace localmind;

namespace {
const char* kRecipes =
    "Pancakes need flour, milk, eggs and a pinch of salt. Whisk everything and rest the "
    "batter for ten minutes.\n\nCook each pancake on a hot buttered pan until golden.";
const char* kSpace =
    "Mars is the fourth planet from the Sun. Its red colour comes from iron oxide dust. "
    "Olympus Mons on Mars is the tallest volcano in the solar system.";
} // namespace

TEST_CASE("engine answers extractively without an LLM") {
    HashingEmbedder embedder;
    VectorStore store(":memory:");
    QueryEngine engine(embedder, store, nullptr);

    auto empty = engine.ask("anything?");
    CHECK(empty.mode == "empty");
    CHECK(empty.sources.empty());

    engine.indexText("recipes.md", kRecipes);
    engine.indexText("space.txt", kSpace);

    auto a = engine.ask("What is the tallest volcano on Mars?");
    CHECK(a.mode == "extractive");
    REQUIRE_FALSE(a.sources.empty());
    CHECK(a.sources[0].documentName == "space.txt");
    CHECK(a.text.find("Olympus Mons") != std::string::npos);
    CHECK(a.text.find("[1]") != std::string::npos);

    auto none = engine.ask("zebra quantum saxophone");
    CHECK(none.mode == "empty");
}

TEST_CASE("engine uses the LLM and falls back when it fails") {
    HashingEmbedder embedder;
    VectorStore store(":memory:");
    testutil::FakeLlm llm;
    QueryEngine engine(embedder, store, &llm);
    engine.indexText("space.txt", kSpace);

    auto a = engine.ask("Why is Mars red?");
    CHECK(a.mode == "llm");
    CHECK(a.text == "canned answer [1]");
    CHECK(llm.lastPrompt.find("[1] (from \"space.txt\")") != std::string::npos);
    CHECK(llm.lastPrompt.find("Question: Why is Mars red?") != std::string::npos);

    llm.fail = true;
    auto b = engine.ask("Why is Mars red?");
    CHECK(b.mode == "extractive");
    CHECK(b.warning.find("model offline") != std::string::npos);
    CHECK(b.text.find("iron oxide") != std::string::npos);
}

TEST_CASE("indexing deduplicates identical content and rejects empty text") {
    HashingEmbedder embedder;
    VectorStore store(":memory:");
    QueryEngine engine(embedder, store, nullptr);

    auto first = engine.indexText("a.txt", kSpace);
    auto second = engine.indexText("copy.txt", std::string(kSpace) + "\r\n\r\n");
    CHECK_FALSE(first.duplicate);
    CHECK(second.duplicate);
    CHECK(second.document.id == first.document.id);
    CHECK(store.stats().documents == 1);

    CHECK_THROWS_AS(engine.indexText("blank.txt", " \n\t "), std::invalid_argument);
}

TEST_CASE("indexPath walks directories and skips unsupported files") {
    testutil::TempDir dir;
    std::filesystem::create_directories(dir.path() / "sub");
    std::filesystem::create_directories(dir.path() / ".hidden");
    std::ofstream(dir.file("one.md")) << kRecipes;
    std::ofstream(dir.file("sub/two.txt")) << kSpace;
    std::ofstream(dir.file("image.png")) << "binary";
    std::ofstream(dir.file(".hidden/secret.txt")) << "secret text";

    HashingEmbedder embedder;
    VectorStore store(":memory:");
    QueryEngine engine(embedder, store, nullptr);
    auto results = engine.indexPath(dir.path().string());
    CHECK(results.size() == 2);
    CHECK(store.stats().documents == 2);
    CHECK_THROWS(engine.indexPath(dir.file("missing")));
}

TEST_CASE("syncEmbeddings re-embeds vectors from another embedder") {
    VectorStore store(":memory:");
    HashingEmbedder small(64);
    QueryEngine(small, store, nullptr).indexText("space.txt", kSpace);
    CHECK(store.embedderId() == "hashing-v1-64");

    HashingEmbedder big(512);
    QueryEngine engine(big, store, nullptr);
    CHECK(engine.syncEmbeddings() > 0);
    CHECK(store.embedderId() == "hashing-v1-512");
    CHECK(engine.syncEmbeddings() == 0);
    CHECK(engine.ask("tallest volcano").mode == "extractive");
}
