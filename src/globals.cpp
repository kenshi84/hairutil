#include "io.h"

namespace globals {
    // Given as command line arguments
    std::string input_file;
    std::string output_ext;
    bool overwrite;

    // Other global variables
    std::string input_file_wo_ext;
    std::string input_ext;
    std::function<std::string(void)> output_file;        // For lazy evaluation
    std::function<void(void)> check_error;
    std::function<std::shared_ptr<cyHairFile>(std::shared_ptr<cyHairFile>)> cmd_exec;

    const std::unordered_map<std::string, std::pair<::io::load_func_t, ::io::save_func_t>> supported_ext = {
        {"bin", {::io::load_bin, ::io::save_bin}},
        {"hair", {::io::load_hair, ::io::save_hair}},
        {"data", {::io::load_data, ::io::save_data}},
        {"ma", {::io::load_ma, ::io::save_ma}}
    };
}
