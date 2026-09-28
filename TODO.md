# LocalMind roadmap

## Done
- [x] Real HTTP server (cpp-httplib) and JSON (nlohmann/json) instead of stubs
- [x] SQLite persistence for documents, chunks and vectors
- [x] Cosine similarity search with top-k and minimum score
- [x] Pluggable embedders: offline hashing embedder and Ollama embeddings
- [x] RAG answers via Ollama chat, with extractive fallback and citations
- [x] `/api/documents` (list, add, get, delete), `/api/stats`, `/api/health`
- [x] Web UI: upload, paste, document list, citations, loading and error states
- [x] Content-hash deduplication and automatic re-embedding when the embedder changes
- [x] Config via flags and environment variables, graceful shutdown
- [x] Unit + API tests, sanitizer build, CI, Docker and docker-compose

## Next
- [ ] Stream LLM answers to the UI (Ollama `stream: true` + chunked HTTP responses)
- [ ] PDF support (server-side text extraction, or PDF.js in the browser)
- [ ] Optional API-key authentication for non-local deployments
- [ ] Hybrid retrieval: BM25 keyword scores combined with vector scores
- [ ] Re-ranking of retrieved chunks
- [ ] ANN index (FAISS / HNSW) for large collections
- [ ] Conversation memory (follow-up questions)
- [ ] Prometheus metrics endpoint
- [ ] devcontainer.json for Codespaces
