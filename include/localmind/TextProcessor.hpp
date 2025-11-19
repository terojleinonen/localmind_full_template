#pragma once
#include <string>
#include <vector>

namespace localmind {

class TextProcessor {
public:
    static std::string readFile(const std::string& path);
    static std::vector<std::string> chunk(const std::string& text,
                                          std::size_t maxChunkSize = 512);
};

} // namespace localmind
