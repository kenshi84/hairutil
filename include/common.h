#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <random>
#include <optional>
#include <numbers>

#include <args.hxx>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <Eigen/Geometry>
#include <nlohmann/json.hpp>
#include <scope_guard.hpp>

#define CY_NO_INTRIN_H
#include <cyHairFile.h>

#include "output_file.h"
#include "random.h"

namespace globals {
    extern const float pi;

    // Given as command line arguments
    extern std::string input_file;
    extern std::set<std::string> output_exts;
    extern bool overwrite;
    extern unsigned int ply_load_default_nsegs;
    extern bool ply_save_ascii;

    extern std::string input_file_wo_ext;
    extern std::string input_ext;
    extern OutputFile output_file_wo_ext;        // For lazy evaluation
    extern std::string output_dir;
    extern std::function<void(void)> check_error;
    extern std::shared_ptr<cyHairFile> (*cmd_exec)(std::shared_ptr<cyHairFile>);
    extern std::mt19937 rng;
    extern const char* const VERSIONTAG;
    extern nlohmann::json json;

    void clear();
}

using std::cout;
using std::endl;

template <typename... Args>
inline void log_debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlog::debug(fmt, std::forward<Args>(args)...);
    globals::json["log"]["debug"].push_back(fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void log_info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlog::info(fmt, std::forward<Args>(args)...);
    globals::json["log"]["info"].push_back(fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void log_warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlog::warn(fmt, std::forward<Args>(args)...);
    globals::json["log"]["warn"].push_back(fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void log_error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlog::error(fmt, std::forward<Args>(args)...);
    globals::json["log"]["error"].push_back(fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void log_critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlog::critical(fmt, std::forward<Args>(args)...);
    globals::json["log"]["critical"].push_back(fmt::format(fmt, std::forward<Args>(args)...));
}
