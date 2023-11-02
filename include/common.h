#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <numeric>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

#include <args.hxx>
#include <spdlog/spdlog.h>
#include <Eigen/Geometry>

#define CY_NO_INTRIN_H
#include <cyHairFile.h>

namespace globals {
    // Given as command line arguments
    extern std::string input_file;
    extern std::string output_ext;
    extern bool overwrite;
    extern unsigned int ply_load_default_nsegs;
    extern bool ply_save_binary;

    extern std::string input_file_wo_ext;
    extern std::string input_ext;
    extern std::function<std::string(void)> output_file;        // For lazy evaluation
    extern std::function<void(void)> check_error;
    extern std::shared_ptr<cyHairFile> (*cmd_exec)(std::shared_ptr<cyHairFile>);
}

using std::cout;
using std::endl;
