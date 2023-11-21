#include "io.h"

namespace globals {
    // Given as command line arguments
    std::string input_file;
    std::string output_ext;
    bool overwrite;
    unsigned int ply_load_default_nsegs;
    bool ply_save_ascii;

    // Other global variables
    std::string input_file_wo_ext;
    std::string input_ext;
    std::function<std::string(void)> output_file;        // For lazy evaluation
    std::function<void(void)> check_error;
    std::shared_ptr<cyHairFile> (*cmd_exec)(std::shared_ptr<cyHairFile>) = nullptr;
    std::mt19937 rng;

    const std::unordered_map<std::string, std::pair<::io::load_func_t, ::io::save_func_t>> supported_ext = {
        {"bin", {::io::load_bin, ::io::save_bin}},
        {"hair", {::io::load_hair, ::io::save_hair}},
        {"data", {::io::load_data, ::io::save_data}},
        {"ply", {::io::load_ply, ::io::save_ply}},
        {"ma", {::io::load_ma, ::io::save_ma}},
        {"abc", {::io::load_abc, ::io::save_abc}}
    };

    ::io::load_func_t load_func;
    ::io::save_func_t save_func;

    void clear() {
        input_file = {};
        output_ext = {};
        overwrite = {};
        ply_load_default_nsegs = {};
        ply_save_ascii = {};
        input_file_wo_ext = {};
        input_ext = {};
        output_file = {};
        check_error = {};
        cmd_exec = nullptr;
        rng = {};
        load_func = {};
        save_func = {};
    }
}
