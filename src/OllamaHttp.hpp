#pragma once
// Internal helper shared by the Ollama embedder and chat client.
#include <string>

#include "cpp-httplib/httplib.h"
#include "json/json.hpp"

namespace localmind::detail {

// POSTs JSON to an Ollama endpoint and returns the parsed response.
// Throws std::runtime_error with a readable message on any failure.
inline nlohmann::json ollamaPost(const std::string& baseUrl, const std::string& path,
                                 const nlohmann::json& body, int timeoutSec) {
    httplib::Client cli(baseUrl);
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(timeoutSec, 0);
    cli.set_write_timeout(timeoutSec, 0);

    auto res = cli.Post(path, body.dump(), "application/json");
    if (!res)
        throw std::runtime_error("Ollama unreachable at " + baseUrl + " (" +
                                 httplib::to_string(res.error()) + ")");
    nlohmann::json out = nlohmann::json::parse(res->body, nullptr, false);
    if (res->status != 200) {
        std::string detail = (!out.is_discarded() && out.contains("error"))
                                 ? out["error"].get<std::string>()
                                 : res->body.substr(0, 200);
        throw std::runtime_error("Ollama " + path + " returned HTTP " +
                                 std::to_string(res->status) + ": " + detail);
    }
    if (out.is_discarded())
        throw std::runtime_error("Ollama " + path + " returned invalid JSON");
    return out;
}

} // namespace localmind::detail
