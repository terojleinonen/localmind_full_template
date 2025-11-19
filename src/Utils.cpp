#include "localmind/Utils.hpp"
#include <iostream>

namespace localmind {

void Utils::log(const std::string& msg) {
    std::cout << "[LocalMind] " << msg << std::endl;
}

} // namespace localmind
