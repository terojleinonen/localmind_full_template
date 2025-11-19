#include "localmind/Embedder.hpp"
#include "localmind/Utils.hpp"

namespace localmind {

Embedder::Embedder(const std::string& modelPath)
    : modelPath_(modelPath)
{
    Utils::log("Embedder created with model: " + modelPath_);
}

std::vector<float> Embedder::getEmbedding(const std::string& text) const {
    // TODO: replace with real ONNX Runtime embeddings.
    // Current implementation: simple deterministic "embedding".
    const int dim = 32;
    std::vector<float> vec(dim, 0.0f);
    for (std::size_t i = 0; i < text.size(); ++i) {
        vec[i % dim] += static_cast<unsigned char>(text[i]) / 255.0f;
    }
    return vec;
}

} // namespace localmind
