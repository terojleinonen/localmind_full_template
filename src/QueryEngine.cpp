#include "localmind/QueryEngine.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>

#include "localmind/Log.hpp"
#include "localmind/TextProcessor.hpp"

namespace fs = std::filesystem;

namespace localmind {

namespace {

const char* kSystemPrompt =
    "You are LocalMind, an assistant that answers questions using only the "
    "user's own documents, provided as numbered context passages. Be concise and "
    "factual. Cite the passages you used with bracketed numbers such as [1] or "
    "[2][3]. If the context does not contain the answer, say so plainly instead "
    "of guessing.";

bool isIndexable(const fs::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".txt" || ext == ".md" || ext == ".markdown";
}

} // namespace

QueryEngine::QueryEngine(const Embedder& embedder, VectorStore& store,
                         const LlmClient* llm, EngineOptions options)
    : embedder_(embedder), store_(store), llm_(llm), options_(options) {}

std::size_t QueryEngine::syncEmbeddings() {
    auto current = store_.embedderId();
    if (current == embedder_.id()) return 0;

    auto chunks = store_.allChunks();
    if (chunks.empty()) return 0;

    Log::warn("Stored vectors were built with '" + current + "'; re-embedding " +
              std::to_string(chunks.size()) + " chunks with '" + embedder_.id() + "'");
    std::vector<std::int64_t> ids;
    std::vector<std::string> texts;
    ids.reserve(chunks.size());
    texts.reserve(chunks.size());
    for (auto& [id, text] : chunks) {
        ids.push_back(id);
        texts.push_back(std::move(text));
    }
    store_.replaceEmbeddings(ids, embedder_.embedBatch(texts), embedder_.id());
    return ids.size();
}

IndexResult QueryEngine::indexText(const std::string& name, const std::string& text) {
    auto clean = TextProcessor::normalize(text);
    if (clean.empty()) throw std::invalid_argument("document has no text content");

    auto hash = TextProcessor::contentHash(clean);
    if (auto existing = store_.findByHash(hash)) return {*existing, true};

    auto chunks = TextProcessor::chunk(clean, options_.chunkSize, options_.chunkOverlap);
    auto embeddings = embedder_.embedBatch(chunks);
    try {
        auto doc = store_.addDocument(name, hash, static_cast<std::int64_t>(clean.size()),
                                      chunks, embeddings, embedder_.id());
        Log::info("Indexed '" + name + "' (" + std::to_string(chunks.size()) + " chunks)");
        return {doc, false};
    } catch (const std::runtime_error&) {
        // Lost a race with an identical concurrent upload (UNIQUE content_hash).
        if (auto existing = store_.findByHash(hash)) return {*existing, true};
        throw;
    }
}

IndexResult QueryEngine::indexFile(const std::string& path) {
    return indexText(fs::path(path).filename().string(), TextProcessor::readFile(path));
}

std::vector<IndexResult> QueryEngine::indexPath(const std::string& path) {
    std::vector<IndexResult> results;
    std::vector<fs::path> files;

    if (fs::is_directory(path)) {
        for (auto it = fs::recursive_directory_iterator(
                 path, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {
            const auto& p = it->path();
            if (p.filename().string().rfind('.', 0) == 0) { // hidden
                if (it->is_directory()) it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file() && isIndexable(p)) files.push_back(p);
        }
        std::sort(files.begin(), files.end());
    } else if (fs::is_regular_file(path)) {
        files.emplace_back(path);
    } else {
        throw std::runtime_error("no such file or directory: " + path);
    }

    for (const auto& f : files) {
        try {
            results.push_back(indexFile(f.string()));
        } catch (const std::exception& e) {
            Log::warn("Skipping " + f.string() + ": " + e.what());
        }
    }
    return results;
}

std::vector<SearchResult> QueryEngine::search(const std::string& question, int topK) const {
    if (topK <= 0) topK = options_.topK;
    return store_.search(embedder_.embed(question), topK, options_.minScore);
}

Answer QueryEngine::ask(const std::string& question, int topK) const {
    Answer a;
    a.sources = search(question, topK);

    if (a.sources.empty()) {
        a.mode = "empty";
        a.text = store_.stats().documents == 0
                     ? "No documents are indexed yet. Add a document and ask again."
                     : "I couldn't find anything relevant to that in your documents.";
        return a;
    }

    if (llm_) {
        try {
            a.text = llm_->generate(kSystemPrompt, buildPrompt(question, a.sources));
            a.mode = "llm";
            return a;
        } catch (const std::exception& e) {
            a.warning = std::string("LLM unavailable, showing extracted passages: ") + e.what();
            Log::warn(a.warning);
        }
    }
    a.text = extractiveAnswer(question, a.sources);
    a.mode = "extractive";
    return a;
}

std::string QueryEngine::buildPrompt(const std::string& question,
                                     const std::vector<SearchResult>& sources) {
    std::ostringstream p;
    p << "Context passages:\n\n";
    for (std::size_t i = 0; i < sources.size(); ++i)
        p << "[" << i + 1 << "] (from \"" << sources[i].documentName << "\")\n"
          << sources[i].text << "\n\n";
    p << "Question: " << question << "\n\n"
      << "Answer using only the context passages above and cite them by number.";
    return p.str();
}

std::string QueryEngine::extractiveAnswer(const std::string& question,
                                          const std::vector<SearchResult>& sources) {
    std::set<std::string> terms;
    for (auto& t : TextProcessor::tokenize(question))
        if (!TextProcessor::isStopword(t)) terms.insert(t);

    struct Candidate {
        double score;
        std::size_t source;
        std::string sentence;
    };
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        for (auto& s : TextProcessor::sentences(sources[i].text)) {
            auto tokens = TextProcessor::tokenize(s);
            if (tokens.size() < 5) continue; // headings and fragments make poor answers
            std::set<std::string> seen;
            for (auto& t : tokens)
                if (terms.count(t)) seen.insert(t);
            if (seen.empty()) continue;
            double score = static_cast<double>(seen.size()) / terms.size() +
                           0.25 * sources[i].score;
            if (s.size() > 400) s = s.substr(0, s.rfind(' ', 400)) + " ...";
            candidates.push_back({score, i, s});
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    std::ostringstream out;
    std::set<std::string> used;
    int taken = 0;
    for (const auto& c : candidates) {
        if (taken == 3) break;
        if (c.score < 0.6 * candidates.front().score) break; // weak matches add noise
        // Chunks overlap, so the same sentence (or a clipped copy) can recur.
        bool repeat = std::any_of(used.begin(), used.end(), [&](const std::string& u) {
            return u.find(c.sentence) != std::string::npos ||
                   c.sentence.find(u) != std::string::npos;
        });
        if (repeat) continue;
        used.insert(c.sentence);
        out << (taken ? "\n\n" : "") << c.sentence << " [" << c.source + 1 << "]";
        ++taken;
    }
    if (taken > 0) return out.str();

    // No sentence shares a term with the question: show the best passage.
    auto text = sources.front().text;
    if (text.size() > 500) text = text.substr(0, text.rfind(' ', 500)) + " ...";
    return text + " [1]";
}

} // namespace localmind
