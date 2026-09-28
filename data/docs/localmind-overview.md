# What is LocalMind?

LocalMind is a small, self-hosted question-answering server for your own documents.
You add plain-text or Markdown files, and LocalMind lets you ask questions about them
in natural language. Every answer cites the passages it was built from, so you can
check where the information came from.

LocalMind runs entirely on your own machine. Documents, embeddings and questions never
leave the computer unless you point it at a remote model server yourself. This makes it
suitable for private notes, internal handbooks, meeting minutes and research material.

The server is a single C++17 binary. It stores everything in one SQLite database file,
serves a browser-based chat interface, and exposes a JSON REST API that other tools can
call. There is no Python runtime, no separate vector database and no cloud account to set up.

## Typical uses

- Searching a personal knowledge base of Markdown notes.
- Answering questions about a team handbook or onboarding guide.
- Exploring a folder of meeting notes or interview transcripts.
- Prototyping retrieval-augmented generation (RAG) before investing in heavier infrastructure.
