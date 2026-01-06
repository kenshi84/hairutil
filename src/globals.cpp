#include "cmd.h"
#include "io.h"

namespace globals {
    const float pi = std::acos(-1.0f);
    const float pi_2 = pi / 2.0f;

    // Given as command line arguments
    std::string input_file;
    std::set<std::string> output_exts;
    bool overwrite;
    unsigned int ply_load_default_nsegs;
    bool ply_save_ascii;

    // Other global variables
    std::string input_file_wo_ext;
    std::string input_ext;
    OutputFile output_file_wo_ext;        // For lazy evaluation
    std::string output_dir;
    std::function<void(void)> check_error;
    ::cmd::exec_func_t cmd_exec;
    std::mt19937 rng;
    nlohmann::json json;


    const std::unordered_map<std::string, std::pair<::io::load_func_t, ::io::save_func_t>> supported_ext = {
        {"bin", {::io::load_bin, ::io::save_bin}},
        {"hair", {::io::load_hair, ::io::save_hair}},
        {"data", {::io::load_data, ::io::save_data}},
        {"ply", {::io::load_ply, ::io::save_ply}},
        {"ma", {::io::load_ma, ::io::save_ma}},
        {"abc", {::io::load_abc, ::io::save_abc}},
        {"npy", {::io::load_npy, ::io::save_npy}}
    };

    void clear() {
        input_file = {};
        output_exts = {};
        overwrite = {};
        ply_load_default_nsegs = {};
        ply_save_ascii = {};
        input_file_wo_ext = {};
        input_ext = {};
        output_file_wo_ext = OutputFile{};
        check_error = {};
        cmd_exec = nullptr;
        rng = {};
        json = {};
    }
}
