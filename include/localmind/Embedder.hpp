#pragma once
#include <string>
#include <vector>

namespace localmind {

// Placeholder embedder.
// TODO: plug in ONNX Runtime here.
class Embedder {
public:
    explicit Embedder(const std::string& modelPath);
    std::vector<float> getEmbedding(const std::string& text) const;

private:
    std::string modelPath_;
};

} // namespace localmind
