# 🧠 LocalMind – Full Template Repository

LocalMind is a **local AI knowledge base** template project written in **C++17** with a simple
web UI. This template gives you a clean architecture to plug in:

- ONNX Runtime (for embeddings)
- FAISS (for vector search)
- SQLite (for document metadata)
- Ollama / Llama.cpp (for answer generation)

Out of the box, this template:

- Builds a C++ HTTP server (`localmind_server`)
- Provides a simple in-memory vector store (no external deps)
- Offers `/index` and `/query` REST endpoints
- Serves a minimal chat-like web UI (HTML/CSS/JS)
- Has clear TODOs where you plug in real AI & databases

## 🔧 Build & Run (C++ server)

```bash
mkdir build && cd build
cmake ..
cmake --build .
./localmind_server
```

The server listens on `http://localhost:8080`.

## 🌐 Run the web UI

From the `web/` folder:

```bash
cd web
python3 -m http.server 3000
```

Then open: **http://localhost:3000**

The UI will talk to the C++ server at `http://localhost:8080`.

## 🧱 Project Structure

```text
localmind/
├── CMakeLists.txt
├── README.md
├── TODO.md
├── src/
│   ├── main.cpp          # entry – starts HTTP server
│   ├── Server.cpp        # REST API (httplib)
│   ├── Embedder.cpp      # placeholder embedding logic
│   ├── VectorStore.cpp   # in-memory vector store
│   ├── QueryEngine.cpp   # ties embedder + store
│   ├── TextProcessor.cpp # file reading & chunking
│   └── Utils.cpp         # simple logging
├── include/
│   └── localmind/
│       ├── Embedder.hpp
│       ├── VectorStore.hpp
│       ├── QueryEngine.hpp
│       ├── TextProcessor.hpp
│       ├── Server.hpp
│       └── Utils.hpp
├── third_party/
│   ├── cpp-httplib/      # single-header HTTP library
│   │   └── httplib.h
│   └── json/
│       └── json.hpp      # nlohmann::json single header
├── data/
│   ├── models/
│   ├── db/
│   └── docs/
└── web/
    ├── index.html
    ├── style.css
    └── script.js
```

## 🚀 Next Steps (AI + DB)

Check `TODO.md` for a roadmap to:

- Replace the dummy `Embedder` with ONNX Runtime
- Replace `VectorStore` with FAISS + SQLite
- Integrate a local LLM (Ollama / Llama.cpp) in `Server.cpp`
- Add PDF.js + Tesseract.js on the frontend for PDF + OCR
