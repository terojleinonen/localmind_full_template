# 📝 LocalMind – TODO Roadmap

## Core AI / Retrieval
- [ ] Replace `Embedder` dummy implementation with ONNX Runtime
- [ ] Replace `VectorStore` in-memory storage with FAISS index
- [ ] Add SQLite database for:
  - documents (filename, summary, stats)
  - chunks (doc_id, text, vector id)
- [ ] Implement proper cosine similarity or FAISS search for embeddings

## HTTP API
- [ ] Add `/documents` endpoint to list all indexed docs
- [ ] Add `/delete/:id` endpoint to remove documents
- [ ] Add `/stats` endpoint for debugging and monitoring

## Web UI
- [ ] Wire UI to real AI answers (not the stub)
- [ ] Add document sidebar with per-file summaries
- [ ] Add file upload and PDF support
- [ ] Integrate PDF.js + Tesseract.js for PDF + OCR
- [ ] Add loading indicators and error handling

## AI / LLM
- [ ] Connect to Ollama or local Llama.cpp via HTTP or C API
- [ ] Implement retrieval-augmented generation (RAG):
  - embed question
  - find top-k chunks
  - build context
  - send to LLM
- [ ] Add configurable models (small / large)

## Dev Experience
- [ ] Add a devcontainer.json for GitHub Codespaces
- [ ] Add unit tests for Embedder, VectorStore, QueryEngine
- [ ] Add CI workflow for building and running tests
