#pragma once
#include <string>

namespace localmind {

class ServerApp {
public:
    ServerApp(const std::string& host, int port);

    // Starts the HTTP server (blocking).
    void run();

private:
    std::string host_;
    int port_;
};

} // namespace localmind
