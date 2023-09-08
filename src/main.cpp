#include "cmd.h"
#include "io.h"

int main(int argc, const char **argv)
{
    args::ArgumentParser parser("Command-line tool for handling hair files");

    args::Group grp_commands(parser, "Commands:");
    args::Command cmd_convert(grp_commands, "convert", "Convert file type", cmd::parse::convert);
    args::Command cmd_subsample(grp_commands, "subsample", "Subsample strands", cmd::parse::subsample);

    args::Group grp_globals("Common arguments:");
    args::ValueFlag<std::string> globals_input_file(grp_globals, "PATH", "(*)Input file", {'i', "input-file"}, args::Options::Required);
    args::ValueFlag<std::string> globals_output_ext(grp_globals, "EXT", "(*)Output file extension {bin,hair,data,ma}", {'o', "output-ext"}, args::Options::Required);
    args::Flag globals_overwrite(grp_globals, "overwrite", "Overwrite when output file exists", {"overwrite"});
    args::ValueFlag<unsigned int> globals_ply_load_default_nsegs(grp_globals, "N", "Default number of segments per strand for PLY files [0]", {"ply-load-default-nsegs"}, 0);
    args::Flag globals_ply_save_binary(grp_globals, "ply-save-binary", "Save PLY files in binary format", {"ply-save-binary"});
    args::ValueFlag<std::string> globals_verbosity(grp_globals, "NAME", "Verbosity level name {trace,debug,info,warn,error,critical,off} [info]", {'v', "verbosity"}, "info");
    args::HelpFlag globals_help(grp_globals, "help", "Show this help message", {'h', "help"});

    args::GlobalOptions global_options(parser, grp_globals);

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cout << parser;
    }
    catch (args::Error& e)
    {
        std::cerr << e.what() << std::endl << parser;
        return 1;
    }

    if (!globals::cmd_exec) {
        spdlog::error("No command specified");
        return 0;
    }

    spdlog::info("verbosity: {}", *globals_verbosity);
    spdlog::set_level(
        *globals_verbosity == "trace" ? spdlog::level::trace :
        *globals_verbosity == "debug" ? spdlog::level::debug :
        *globals_verbosity == "info" ? spdlog::level::info :
        *globals_verbosity == "warn" ? spdlog::level::warn :
        *globals_verbosity == "error" ? spdlog::level::err :
        *globals_verbosity == "critical" ? spdlog::level::critical : spdlog::level::off
    );

    globals::input_file = *globals_input_file;
    globals::output_ext = *globals_output_ext;
    globals::overwrite = globals_overwrite;
    globals::ply_load_default_nsegs = *globals_ply_load_default_nsegs;
    globals::ply_save_binary = globals_ply_save_binary;

    // Get file extension from globals::input_file, in lowercase
    globals::input_ext = globals::input_file.substr(globals::input_file.find_last_of(".") + 1);
    std::transform(globals::input_ext.begin(), globals::input_ext.end(), globals::input_ext.begin(), [](unsigned char c){ return std::tolower(c); });

    // Check if input file extension is supported
    if (globals::supported_ext.count(globals::input_ext) == 0) {
        spdlog::error("Unsupported input file extension: {}", globals::input_ext);
        return 1;
    }
    io::load_func_t load_func = globals::supported_ext.at(globals::input_ext).first;

    // Check if output file extension is supported
    if (globals::supported_ext.count(globals::output_ext) == 0) {
        spdlog::error("Unsupported output file extension: {}", globals::output_ext);
        return 1;
    }
    io::save_func_t save_func = globals::supported_ext.at(globals::output_ext).second;

    // Get input file name without extension
    globals::input_file_wo_ext = globals::input_file.substr(0, globals::input_file.find_last_of("."));

    // Check if the output file already exists
    if (!globals::overwrite && std::filesystem::exists(globals::output_file())) {
        spdlog::error("Output file already exists: {}", globals::output_file());
        spdlog::error("Use --overwrite to overwrite the file");
        return 1;
    }

    try
    {
        // Perform precursory checks, if any
        if (globals::check_error)
            globals::check_error();

        spdlog::info("Loading from {} ...", globals::input_file);
        auto hairfile_in = load_func(globals::input_file);

        spdlog::info("Number of strands: {}", hairfile_in->GetHeader().hair_count);
        spdlog::info("Number of points: {}", hairfile_in->GetHeader().point_count);

        auto hairfile_out = globals::cmd_exec(hairfile_in);

        spdlog::info("Saving to {} ...", globals::output_file());
        save_func(globals::output_file(), hairfile_out);
    }
    catch (const std::exception &e)
    {
        spdlog::error("{}", e.what());
    }

    spdlog::info("Done");
    return 0;
}
