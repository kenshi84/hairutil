#include "cmd.h"

void cmd::parse::convert(args::Subparser &parser) {
    parser.Parse();
    globals::cmd_exec = cmd::exec::convert;
    globals::output_file = [](){ return globals::input_file_wo_ext + "." + globals::output_ext; };
    globals::check_error = [](){
        if (globals::input_ext == globals::output_ext) {
            spdlog::error("Input and output file extensions are the same: {}", globals::input_ext);
            throw;
        }
    };
}

std::shared_ptr<cyHairFile> cmd::exec::convert(std::shared_ptr<cyHairFile> hairfile_in) {
    return hairfile_in;
}
