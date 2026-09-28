#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "localmind/Log.hpp"

int main(int argc, char** argv) {
    localmind::Log::setLevel(localmind::LogLevel::Error); // keep test output clean
    doctest::Context ctx(argc, argv);
    return ctx.run();
}
