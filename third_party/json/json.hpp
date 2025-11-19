// NOTE: This is a placeholder for nlohmann::json.
// In a real project, replace with the official single-header:
// https://github.com/nlohmann/json
#pragma once
#include <string>
#include <map>

namespace nlohmann {

class json : public std::map<std::string, std::string> {
public:
    static json parse(const std::string&) { return {}; }
    std::string dump() const { return "{}"; }

    template<typename T>
    T value(const std::string&, const T& def) const { return def; }
};

} // namespace nlohmann
