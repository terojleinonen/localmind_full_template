#include "localmind/TextProcessor.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace localmind {

namespace {

bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

// Length of a valid UTF-8 sequence starting at s[i], or 0 if invalid.
std::size_t utf8SeqLen(const std::string& s, std::size_t i) {
    auto c = static_cast<unsigned char>(s[i]);
    std::size_t len;
    if (c < 0x80) return 1;
    if (c >= 0xC2 && c <= 0xDF) len = 2;
    else if (c >= 0xE0 && c <= 0xEF) len = 3;
    else if (c >= 0xF0 && c <= 0xF4) len = 4;
    else return 0;
    if (i + len > s.size()) return 0;
    for (std::size_t k = 1; k < len; ++k)
        if (!isContinuation(static_cast<unsigned char>(s[i + k]))) return 0;
    auto c1 = static_cast<unsigned char>(s[i + 1]);
    if (c == 0xE0 && c1 < 0xA0) return 0; // overlong
    if (c == 0xED && c1 > 0x9F) return 0; // surrogates
    if (c == 0xF0 && c1 < 0x90) return 0; // overlong
    if (c == 0xF4 && c1 > 0x8F) return 0; // > U+10FFFF
    return len;
}

// Moves i backwards until it is not in the middle of a code point.
std::size_t floorBoundary(const std::string& s, std::size_t i) {
    while (i > 0 && i < s.size() && isContinuation(static_cast<unsigned char>(s[i]))) --i;
    return i;
}

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\n");
    return s.substr(b, e - b + 1);
}

bool isWordByte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c >= 0x80;
}

// Finds the best place to end a chunk within text[lo, hi): the end of the
// last paragraph, else sentence, else word. Returns hi if none is found.
std::size_t findBreak(const std::string& text, std::size_t lo, std::size_t hi) {
    auto lastOf = [&](const std::string& needle) -> std::size_t {
        if (hi < needle.size()) return std::string::npos;
        auto p = text.rfind(needle, hi - needle.size());
        return (p != std::string::npos && p >= lo) ? p + needle.size() : std::string::npos;
    };
    if (auto p = lastOf("\n\n"); p != std::string::npos) return p;

    std::size_t best = std::string::npos;
    for (const char* s : {". ", "! ", "? ", ".\n", "!\n", "?\n", "\n"}) {
        auto p = lastOf(s);
        if (p != std::string::npos && (best == std::string::npos || p > best)) best = p;
    }
    if (best != std::string::npos) return best;

    if (auto p = lastOf(" "); p != std::string::npos) return p;
    return hi;
}

} // namespace

std::string TextProcessor::readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string TextProcessor::normalize(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    int newlines = 0;
    bool pendingSpace = false;

    for (std::size_t i = 0; i < text.size();) {
        auto c = static_cast<unsigned char>(text[i]);

        if (c == '\r' || c == '\n') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            ++i;
            pendingSpace = false;
            // Strip trailing spaces on the line we are closing.
            while (!out.empty() && out.back() == ' ') out.pop_back();
            if (!out.empty() && newlines < 2) {
                out += '\n';
                ++newlines;
            }
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
            pendingSpace = true;
            ++i;
            continue;
        }
        if (c < 0x20 || c == 0x7F) { // other control characters
            ++i;
            continue;
        }

        std::size_t len = utf8SeqLen(text, i);
        if (len == 0) { // invalid byte: drop it
            ++i;
            continue;
        }
        if (pendingSpace && newlines == 0 && !out.empty()) out += ' ';
        pendingSpace = false;
        newlines = 0;
        out.append(text, i, len);
        i += len;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return out;
}

std::vector<std::string> TextProcessor::chunk(const std::string& text,
                                              std::size_t maxChunkSize,
                                              std::size_t overlap) {
    std::vector<std::string> chunks;
    if (maxChunkSize == 0) return chunks;
    if (overlap >= maxChunkSize) overlap = maxChunkSize / 4;

    const std::size_t n = text.size();
    std::size_t pos = 0;
    while (pos < n) {
        std::size_t end = std::min(pos + maxChunkSize, n);
        if (end < n) {
            end = findBreak(text, pos + maxChunkSize / 2, end);
            end = floorBoundary(text, end);
            if (end <= pos) end = std::min(pos + maxChunkSize, n); // pathological
        }

        auto piece = trim(text.substr(pos, end - pos));
        if (!piece.empty()) chunks.push_back(std::move(piece));
        if (end >= n) break;

        // Start the next chunk `overlap` bytes back, aligned to a word start.
        std::size_t next = end > overlap ? end - overlap : 0;
        if (next <= pos) next = end;
        if (next < end) {
            auto sp = text.find_first_of(" \n", next);
            next = (sp != std::string::npos && sp < end) ? sp + 1 : end;
        }
        next = floorBoundary(text, next);
        if (next <= pos) next = end;
        pos = next;
    }
    return chunks;
}

std::vector<std::string> TextProcessor::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&] {
        bool keep = cur.size() >= 2 || (cur.size() == 1 && cur[0] >= '0' && cur[0] <= '9');
        if (keep) tokens.push_back(cur);
        cur.clear();
    };
    for (char ch : text) {
        auto c = static_cast<unsigned char>(ch);
        if (isWordByte(c)) {
            cur += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : ch;
        } else if (!cur.empty()) {
            flush();
        }
    }
    if (!cur.empty()) flush();
    return tokens;
}

bool TextProcessor::isStopword(const std::string& token) {
    static const std::unordered_set<std::string> stop = {
        "a", "about", "above", "after", "again", "all", "am", "an", "and", "any",
        "are", "as", "at", "be", "because", "been", "before", "being", "below",
        "between", "both", "but", "by", "can", "could", "did", "do", "does",
        "doing", "down", "during", "each", "few", "for", "from", "further", "had",
        "has", "have", "having", "he", "her", "here", "hers", "him", "his", "how",
        "i", "if", "in", "into", "is", "it", "its", "itself", "just", "me", "more",
        "most", "my", "no", "nor", "not", "now", "of", "off", "on", "once", "only",
        "or", "other", "our", "ours", "out", "over", "own", "same", "she", "should",
        "so", "some", "such", "than", "that", "the", "their", "theirs", "them",
        "then", "there", "these", "they", "this", "those", "through", "to", "too",
        "under", "until", "up", "very", "was", "we", "were", "what", "when",
        "where", "which", "while", "who", "whom", "why", "will", "with", "would",
        "you", "your", "yours", "tell", "please", "explain", "describe",
    };
    return stop.count(token) > 0;
}

std::vector<std::string> TextProcessor::sentences(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&] {
        if (auto t = trim(cur); !t.empty()) out.push_back(t);
        cur.clear();
    };

    // Hard-wrapped lines are joined; blank lines, headings and list items
    // start a new block. Markdown markers are stripped.
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty()) {
            flush();
            continue;
        }
        const bool heading = line[0] == '#';
        std::size_t skip = 0;
        if (heading || line[0] == '>') {
            skip = line.find_first_not_of("#> ");
        } else if ((line[0] == '-' || line[0] == '*' || line[0] == '+') && line.size() > 1 &&
                   line[1] == ' ') {
            skip = 2;
        } else {
            auto d = line.find_first_not_of("0123456789");
            if (d > 0 && d != std::string::npos && d + 1 < line.size() &&
                (line[d] == '.' || line[d] == ')') && line[d + 1] == ' ')
                skip = d + 2;
        }
        if (skip > 0 || heading) flush();
        if (skip == std::string::npos) continue;
        line = line.substr(skip);

        if (!cur.empty()) cur += ' ';
        for (std::size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            cur += c;
            bool terminal = c == '.' || c == '!' || c == '?';
            if (terminal && (i + 1 >= line.size() || line[i + 1] == ' ')) flush();
        }
        if (heading) flush(); // a heading is its own block
    }
    flush();
    return out;
}

std::string TextProcessor::contentHash(const std::string& text) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : text) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

} // namespace localmind
