#include "doctest/doctest.h"

#include "localmind/TextProcessor.hpp"

using localmind::TextProcessor;

TEST_CASE("normalize cleans whitespace, line endings and invalid UTF-8") {
    CHECK(TextProcessor::normalize("  a\t\tb  \r\nc\r\n\r\n\r\n\r\nd  ") == "a b\nc\n\nd");
    CHECK(TextProcessor::normalize("x\x01y\x7fz") == "xyz");
    CHECK(TextProcessor::normalize("caf\xC3\xA9") == "caf\xC3\xA9");      // valid 2-byte
    CHECK(TextProcessor::normalize("bad\xC3(\xFFok") == "bad(ok");         // invalid bytes dropped
    CHECK(TextProcessor::normalize("\n\n  \n") == "");
}

TEST_CASE("chunk respects size limit and covers all text") {
    std::string text;
    for (int i = 0; i < 200; ++i) text += "Sentence number " + std::to_string(i) + " is here. ";
    auto chunks = TextProcessor::chunk(text, 200, 40);
    REQUIRE(chunks.size() > 5);
    for (const auto& c : chunks) CHECK(c.size() <= 200);
    CHECK(chunks.front().rfind("Sentence number 0", 0) == 0);
    CHECK(chunks.back().find("Sentence number 199 is here.") != std::string::npos);
    // Chunks end on sentence boundaries when possible.
    CHECK(chunks.front().back() == '.');
}

TEST_CASE("chunk overlap repeats text between neighbours") {
    std::string text;
    for (int i = 0; i < 50; ++i) text += "word" + std::to_string(i) + " ";
    auto chunks = TextProcessor::chunk(text, 100, 30);
    REQUIRE(chunks.size() >= 2);
    auto tailOfFirst = chunks[0].substr(chunks[0].rfind(' ') + 1);
    CHECK(chunks[1].find(tailOfFirst) != std::string::npos);
}

TEST_CASE("chunk handles short, empty and unbroken input") {
    CHECK(TextProcessor::chunk("", 100, 10).empty());
    CHECK(TextProcessor::chunk("short", 100, 10) == std::vector<std::string>{"short"});
    auto chunks = TextProcessor::chunk(std::string(1000, 'x'), 100, 10);
    CHECK(chunks.size() >= 10);
    for (const auto& c : chunks) CHECK(c.size() <= 100);
}

TEST_CASE("chunk never splits a UTF-8 code point") {
    std::string text;
    for (int i = 0; i < 300; ++i) text += "\xC3\xA4"; // 'ä' without spaces
    for (const auto& c : TextProcessor::chunk(text, 101, 11)) {
        CHECK(TextProcessor::normalize(c) == c); // would drop broken sequences
    }
}

TEST_CASE("tokenize lowercases and splits on punctuation") {
    auto t = TextProcessor::tokenize("Hello, WORLD! It's C++17 a 7");
    CHECK(t == std::vector<std::string>{"hello", "world", "it", "17", "7"});
}

TEST_CASE("sentences splits on terminal punctuation and newlines") {
    auto s = TextProcessor::sentences("One. Two! Three?\nFour 3.14 stays");
    CHECK(s == std::vector<std::string>{"One.", "Two!", "Three?", "Four 3.14 stays"});
}

TEST_CASE("sentences joins wrapped lines and separates Markdown blocks") {
    auto s = TextProcessor::sentences(
        "# Title\nA sentence that is\nhard wrapped. Next one\n\n- item one\n- item two\n"
        "2. numbered\n> quoted");
    CHECK(s == std::vector<std::string>{"Title", "A sentence that is hard wrapped.", "Next one",
                                        "item one", "item two", "numbered", "quoted"});
}

TEST_CASE("contentHash is stable") {
    CHECK(TextProcessor::contentHash("") == "cbf29ce484222325");
    CHECK(TextProcessor::contentHash("a") == "af63dc4c8601ec8c");
    CHECK(TextProcessor::contentHash("a") != TextProcessor::contentHash("b"));
}
