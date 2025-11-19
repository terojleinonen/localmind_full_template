// NOTE: This is a placeholder for cpp-httplib.
// In a real project, replace this file with the official single-header
// from: https://github.com/yhirose/cpp-httplib
#pragma once
#include <functional>
#include <map>
#include <string>

namespace httplib {

struct Request {
    std::string body;
};
struct Response {
    std::string body;
    int status = 200;
    void set_content(const std::string& b, const char*) { body = b; }
    void set_header(const std::string&, const std::string&) {}
};

class Server {
public:
    using Handler = std::function<void(const Request&, Response&)>;
    void Post(const std::string&, Handler) {}
    void listen(const char*, int) {}
};

} // namespace httplib
