# 🧠 LocalMind

**Ask questions about your own documents — fully local, one C++ binary.**

LocalMind is a small retrieval-augmented generation (RAG) server written in C++17. Add
text or Markdown documents, ask questions in a chat UI, and get answers that cite the
passages they came from. Documents, vectors and questions never leave your machine.

- **Single binary + SQLite** — no Python, no separate vector DB, no cloud account.
- **Works offline out of the box** — a built-in lexical embedder and an extractive
  answer mode mean it is useful before you install any model.
- **Better with Ollama** — plug in semantic embeddings (`nomic-embed-text`) and a local
  chat model (`llama3.2`) for fluent, cited answers.
- **Web UI** — drag-and-drop upload, document list, chat with clickable citations,
  light/dark themes, mobile-friendly.
- **REST API**, graceful shutdown, input validation, request limits, tests under
  ASan/UBSan, CI and Docker.

## Quick start

Requirements: a C++17 compiler, CMake ≥ 3.16 and SQLite 3 development headers
(`apt install g++ cmake libsqlite3-dev` on Debian/Ubuntu; included in the Xcode SDK on macOS).

```bash
cmake -S . -B build
cmake --build build -j
./build/localmind_server --ingest data/docs
```

Open **http://localhost:8080** and try *"How does retrieval work?"*.
Run the binary from the repository root so it finds `web/` (or pass `--web-dir`).

### With a local LLM (recommended)

Install [Ollama](https://ollama.com), then:

```bash
ollama pull llama3.2           # chat model
ollama pull nomic-embed-text   # embedding model
./build/localmind_server --embedder ollama --ingest data/docs
```

LocalMind picks Ollama up automatically for answers. If Ollama stops, questions keep
working in extractive mode and the UI shows a warning. Switching embedders later is safe:
stored chunks are re-embedded at startup.

### With Docker

```bash
docker compose up --build      # LocalMind + Ollama; first run downloads the models
```

Or just the server, without an LLM:

```bash
docker build -t localmind .
docker run --rm -p 127.0.0.1:8080:8080 -v localmind-data:/app/data \
  -e LOCALMIND_LLM=none localmind --ingest /app/samples
```

## How it works

```text
            index                                      ask
 text ─▶ normalise ─▶ chunk (800c, 120c overlap)   question ─▶ embed
                          │                                       │
                          ▼                                       ▼
                        embed ─▶ SQLite + in-memory ◀── cosine top-k (min score)
                                 vectors                          │
                                                                  ▼
                                   Ollama chat ◀── numbered context passages
                                        │ (fallback: extractive sentences)
                                        ▼
                                answer with [n] citations
```

| Component | File | Notes |
|---|---|---|
| `TextProcessor` | `src/TextProcessor.cpp` | UTF-8-safe normalisation, paragraph/sentence-aware chunking |
| `Embedder` | `src/Embedder.cpp` | `HashingEmbedder` (offline, lexical) or `OllamaEmbedder` (semantic) |
| `VectorStore` | `src/VectorStore.cpp` | SQLite (WAL) is the source of truth; vectors cached in memory for exact search |
| `QueryEngine` | `src/QueryEngine.cpp` | indexing, dedup by content hash, retrieval, prompting, extractive fallback |
| `LlmClient` | `src/LlmClient.cpp` | Ollama `/api/chat` |
| `ServerApp` | `src/Server.cpp` | REST API + static UI (cpp-httplib) |
| `Config` | `src/Config.cpp` | flags and environment variables |

## Configuration

Every option is a flag or an environment variable (flags win). Run
`./build/localmind_server --help` for the full list. The most useful ones:

| Flag | Env | Default |
|---|---|---|
| `--host` | `LOCALMIND_HOST` | `127.0.0.1` |
| `--port` | `LOCALMIND_PORT` | `8080` |
| `--db` | `LOCALMIND_DB` | `data/db/localmind.db` |
| `--ingest` (repeatable) | `LOCALMIND_INGEST` (`:`-separated) | — |
| `--embedder` | `LOCALMIND_EMBEDDER` | `hashing` (`ollama` for semantic search) |
| `--llm` | `LOCALMIND_LLM` | `ollama` (`none` = extractive only) |
| `--ollama-url` | `OLLAMA_URL` | `http://127.0.0.1:11434` |
| `--max-answer-tokens` | `LOCALMIND_MAX_ANSWER_TOKENS` | `400` (bounds answer length and time) |
| `--chat-model` / `--embed-model` | `LOCALMIND_CHAT_MODEL` / `LOCALMIND_EMBED_MODEL` | `llama3.2` / `nomic-embed-text` |
| `--top-k` / `--min-score` | `LOCALMIND_TOP_K` / `LOCALMIND_MIN_SCORE` | `4` / `0.05` |
| `--chunk-size` / `--chunk-overlap` | `LOCALMIND_CHUNK_SIZE` / `LOCALMIND_CHUNK_OVERLAP` | `800` / `120` |
| `--cors-origin` | `LOCALMIND_CORS_ORIGIN` | off (UI is same-origin) |
| `--log-level` | `LOCALMIND_LOG_LEVEL` | `info` |

## REST API

All responses are JSON; errors look like `{"error": "..."}` with a 4xx/5xx status.

| Method | Path | Body | Result |
|---|---|---|---|
| `GET` | `/api/health` | | status, version, embedder, LLM availability |
| `GET` | `/api/stats` | | document/chunk counts and retrieval settings |
| `GET` | `/api/documents` | | `{documents: [...]}` newest first |
| `POST` | `/api/documents` | `{name, text}` | `201` created, or `200` with `duplicate: true` |
| `GET` | `/api/documents/:id` | | metadata + `chunk_texts` |
| `DELETE` | `/api/documents/:id` | | `204` |
| `POST` | `/api/query` | `{question, top_k?}` | `{answer, mode, sources, took_ms, warning?}` |

```bash
curl -X POST localhost:8080/api/documents -H 'Content-Type: application/json' \
  -d '{"name":"coffee.txt","text":"The espresso machine is descaled every Friday by Maria."}'

curl -X POST localhost:8080/api/query -H 'Content-Type: application/json' \
  -d '{"question":"Who descales the espresso machine?"}'
# {"answer":"The espresso machine is descaled every Friday by Maria. [1]","mode":"extractive",
#  "sources":[{"n":1,"document_name":"coffee.txt","score":0.31,...}],"took_ms":1}
```

`mode` is `llm` (generated), `extractive` (sentences picked from the sources) or `empty`
(nothing relevant found).

## Development

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j && ctest --test-dir build --output-on-failure

# Sanitizers / warnings-as-errors, as in CI
cmake -S . -B build-asan -DLOCALMIND_SANITIZE=ON -DLOCALMIND_WERROR=ON
```

Tests (doctest, `tests/`) cover chunking, embeddings, persistence, the query engine,
config parsing, the HTTP API end to end, and the Ollama client against a fake Ollama
server — no network or models needed. CI (`.github/workflows/ci.yml`) builds and tests on
Linux and macOS, runs a sanitizer build and a smoke test, and builds the Docker image.

```text
include/localmind/   public headers
src/                 implementation (+ main.cpp)
tests/               unit and API tests
web/                 static UI (no build step, no dependencies)
data/docs/           sample documents
third_party/         vendored single headers: cpp-httplib 0.58.0, nlohmann/json 3.12.0, doctest 2.5.3
```

## Deployment notes and limits

- **No authentication.** The server binds to `127.0.0.1` by default. If you expose it
  (`--host 0.0.0.0`, Docker), put it behind a reverse proxy that handles TLS and auth.
- **Scale:** exact in-memory search is comfortable up to tens of thousands of chunks.
  Beyond that, swap `VectorStore::search` for an ANN index (FAISS, HNSW).
- **Formats:** plain text and Markdown. PDFs must be converted to text first.
- **Single process:** one server per database file.
- POSIX only (Linux, macOS); Windows would need a different signal-handling path.

See [TODO.md](TODO.md) for the roadmap.
