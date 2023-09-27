#include "io.h"

namespace globals {
    // Given as command line arguments
    std::string input_file;
    std::string output_ext;
    bool overwrite;
    unsigned int ply_load_default_nsegs;
    bool ply_save_binary;

    // Other global variables
    std::string input_file_wo_ext;
    std::string input_ext;
    std::function<std::string(void)> output_file;        // For lazy evaluation
    std::function<void(void)> check_error;
    std::shared_ptr<cyHairFile> (*cmd_exec)(std::shared_ptr<cyHairFile>) = nullptr;

    const std::unordered_map<std::string, std::pair<::io::load_func_t, ::io::save_func_t>> supported_ext = {
        {"bin", {::io::load_bin, ::io::save_bin}},
        {"hair", {::io::load_hair, ::io::save_hair}},
        {"data", {::io::load_data, ::io::save_data}},
        {"ply", {::io::load_ply, ::io::save_ply}},
        {"ma", {::io::load_ma, ::io::save_ma}}
    };
}
