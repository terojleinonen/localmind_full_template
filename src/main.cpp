#include "localmind/Server.hpp"

int main() {
    localmind::ServerApp app("0.0.0.0", 8080);
    app.run();
    return 0;
}
