# How retrieval works

LocalMind uses retrieval-augmented generation, often shortened to RAG. Instead of asking a
language model to answer from memory, it first retrieves the most relevant passages from
your documents and then asks the model to answer using only those passages.

## Indexing

When a document is added, LocalMind normalises the text (line endings, whitespace and
invalid characters) and splits it into overlapping chunks of roughly 800 characters.
Chunks end on paragraph or sentence boundaries whenever possible, and neighbouring chunks
share about 120 characters so that an idea split across a boundary is not lost.

Each chunk is converted into an embedding: a vector of numbers that represents its meaning.
The chunk text and its embedding are stored in SQLite. Identical documents are detected by a
content hash and are only indexed once.

## Answering a question

1. The question is embedded with the same embedder as the documents.
2. Cosine similarity is computed between the question vector and every chunk vector.
3. The top-k most similar chunks (four by default) above a minimum score are selected.
4. The chunks are numbered and placed into a prompt, and the language model writes an
   answer that cites them as [1], [2] and so on.

If no language model is available, LocalMind falls back to extractive mode: it picks the
sentences from the retrieved chunks that share the most words with the question and
returns them with citations. The answer is less fluent, but always grounded in your text.

## Search performance

All vectors are held in memory and searched by brute force. This is simple and exact, and
comfortably fast for tens of thousands of chunks. Larger collections would benefit from an
approximate nearest-neighbour index such as FAISS or HNSW.
