#include "doctest/doctest.h"

#include <cstdlib>

#include "localmind/Config.hpp"

using namespace localmind;

TEST_CASE("defaults, environment and flags are layered") {
    setenv("LOCALMIND_PORT", "9000", 1);
    setenv("LOCALMIND_TOP_K", "7", 1);

    Config cfg;
    const char* argv[] = {"prog", "--port", "9100", "--llm=none", "--ingest", "a", "--ingest", "b"};
    auto r = parseConfig(8, argv, cfg);
    unsetenv("LOCALMIND_PORT");
    unsetenv("LOCALMIND_TOP_K");

    REQUIRE(r.ok);
    CHECK(cfg.port == 9100); // flag beats env
    CHECK(cfg.topK == 7);    // env beats default
    CHECK(cfg.llm == "none");
    CHECK(cfg.host == "127.0.0.1");
    CHECK(cfg.ingestPaths == std::vector<std::string>{"a", "b"});
}

TEST_CASE("invalid options are reported") {
    auto fails = [](std::vector<const char*> args) {
        Config cfg;
        args.insert(args.begin(), "prog");
        return !parseConfig(static_cast<int>(args.size()), args.data(), cfg).ok;
    };
    CHECK(fails({"--port", "http"}));
    CHECK(fails({"--port", "70000"}));
    CHECK(fails({"--embedder", "magic"}));
    CHECK(fails({"--bogus"}));
    CHECK(fails({"--port"}));
    CHECK(fails({"--chunk-size", "100", "--chunk-overlap", "100"}));
}

TEST_CASE("--help exits early with usage") {
    Config cfg;
    const char* argv[] = {"prog", "--help"};
    auto r = parseConfig(2, argv, cfg);
    CHECK(r.ok);
    CHECK(r.exitNow);
    CHECK(r.message.find("--ollama-url") != std::string::npos);
}
