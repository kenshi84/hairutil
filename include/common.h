#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <cmath>
#include <numeric>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <random>
#include <optional>

#include <args.hxx>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <Eigen/Geometry>

#define CY_NO_INTRIN_H
#include <cyHairFile.h>

#include "output_file.h"
#include "random.h"

namespace globals {
    extern const float pi;

    // Given as command line arguments
    extern std::string input_file;
    extern std::string output_ext;
    extern bool overwrite;
    extern unsigned int ply_load_default_nsegs;
    extern bool ply_save_ascii;

    extern std::string input_file_wo_ext;
    extern std::string input_ext;
    extern OutputFile output_file;        // For lazy evaluation
    extern std::function<void(void)> check_error;
    extern std::shared_ptr<cyHairFile> (*cmd_exec)(std::shared_ptr<cyHairFile>);
    extern std::mt19937 rng;
    extern const char* const VERSIONTAG;

    void clear();
}

using std::cout;
using std::endl;
