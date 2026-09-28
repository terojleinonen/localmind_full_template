#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace localmind {

class TextProcessor {
public:
    // Reads a whole file; throws std::runtime_error if it cannot be opened.
    static std::string readFile(const std::string& path);

    // Normalises line endings, strips control characters and invalid UTF-8,
    // and collapses runs of blank lines.
    static std::string normalize(const std::string& text);

    // Splits text into chunks of at most maxChunkSize bytes, preferring
    // paragraph, sentence and word boundaries, with `overlap` bytes carried
    // over between consecutive chunks. Never splits a UTF-8 code point.
    static std::vector<std::string> chunk(const std::string& text,
                                          std::size_t maxChunkSize = 800,
                                          std::size_t overlap = 120);

    // Lower-cased word tokens (ASCII letters/digits and any non-ASCII bytes).
    static std::vector<std::string> tokenize(const std::string& text);

    static bool isStopword(const std::string& token);

    // Splits text into sentences (roughly, on . ! ? and newlines).
    static std::vector<std::string> sentences(const std::string& text);

    // 64-bit FNV-1a hash, hex encoded. Stable across platforms.
    static std::string contentHash(const std::string& text);
};

} // namespace localmind
