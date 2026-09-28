#pragma once
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

#include "localmind/LlmClient.hpp"

namespace testutil {

// Temporary directory removed on destruction.
class TempDir {
public:
    TempDir() {
        std::random_device rd;
        path_ = std::filesystem::temp_directory_path() /
                ("localmind-test-" + std::to_string(rd()) + std::to_string(rd()));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    std::string file(const std::string& name) const { return (path_ / name).string(); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// LLM double that records the prompt and returns a canned answer.
class FakeLlm : public localmind::LlmClient {
public:
    std::string name() const override { return "fake"; }
    bool available() const override { return !fail; }
    std::string generate(const std::string&, const std::string& prompt) const override {
        lastPrompt = prompt;
        if (fail) throw std::runtime_error("model offline");
        return "canned answer [1]";
    }
    bool fail = false;
    mutable std::string lastPrompt;
};

} // namespace testutil
