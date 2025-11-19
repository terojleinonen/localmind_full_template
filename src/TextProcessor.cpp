#include "localmind/TextProcessor.hpp"
#include <fstream>
#include <sstream>

namespace localmind {

std::string TextProcessor::readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return {};
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> TextProcessor::chunk(const std::string& text,
                                              std::size_t maxChunkSize) {
    std::vector<std::string> chunks;
    for (std::size_t i = 0; i < text.size(); i += maxChunkSize) {
        chunks.push_back(text.substr(i, maxChunkSize));
    }
    return chunks;
}

} // namespace localmind
