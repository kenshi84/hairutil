#pragma once

#include <filesystem>
#include <functional>
#include <string>

struct OutputFile {
    std::function<std::string(void)> func;
    std::string dir;

    // Set default ctors and assignment operators
    OutputFile() = default;
    OutputFile(const OutputFile&) = default;
    OutputFile(OutputFile&&) = default;
    OutputFile& operator=(const OutputFile&) = default;
    OutputFile& operator=(OutputFile&&) = default;

    // Custom operators
    OutputFile& operator=(std::function<std::string(void)> func) {
        this->func = func;
        return *this;
    }
    operator bool() const { return static_cast<bool>(func); }
    std::string operator()() const;
};
