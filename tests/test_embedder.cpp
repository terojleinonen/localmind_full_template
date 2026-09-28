#include "doctest/doctest.h"

#include <cmath>

#include "localmind/Embedder.hpp"

using localmind::HashingEmbedder;

namespace {
float cosine(const std::vector<float>& a, const std::vector<float>& b) {
    float s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}
} // namespace

TEST_CASE("hashing embedder is deterministic and normalised") {
    HashingEmbedder e(256);
    auto a = e.embed("The quick brown fox jumps over the lazy dog");
    auto b = e.embed("The quick brown fox jumps over the lazy dog");
    REQUIRE(a.size() == 256);
    CHECK(a == b);
    CHECK(cosine(a, a) == doctest::Approx(1.0f).epsilon(1e-5));
    CHECK(e.id() == "hashing-v1-256");
}

TEST_CASE("hashing embedder ranks related text higher") {
    HashingEmbedder e;
    auto q = e.embed("How do I reset my password?");
    auto related = e.embed("To reset a forgotten password, open Settings and choose Reset password.");
    auto unrelated = e.embed("The quarterly revenue grew by twelve percent in Europe.");
    CHECK(cosine(q, related) > cosine(q, unrelated) + 0.2f);
}

TEST_CASE("hashing embedder ignores case, stopwords and plurals") {
    HashingEmbedder e;
    CHECK(cosine(e.embed("Documents"), e.embed("the document")) == doctest::Approx(1.0f).epsilon(1e-5));
}

TEST_CASE("empty text yields a zero vector") {
    HashingEmbedder e(64);
    auto v = e.embed("the and of");
    for (float x : v) CHECK(x == 0.0f);
}

TEST_CASE("l2Normalize") {
    std::vector<float> v{3, 4};
    localmind::l2Normalize(v);
    CHECK(v[0] == doctest::Approx(0.6f));
    CHECK(v[1] == doctest::Approx(0.8f));
}
