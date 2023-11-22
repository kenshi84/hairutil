#include <ctime>

#include "cmd.h"
#include "io.h"

#ifdef TEST_MODE
int test_main(int argc, const char **argv)
#else
int main(int argc, const char **argv)
#endif
{
    args::ArgumentParser parser(fmt::format(
        "A command-line tool for handling hair files (version: {})\n"
        "Supported file formats:\n"
        "  .bin\n"
        "  .hair\n"
        "  .data\n"
        "  .ply\n"
        "  .ma\n"
        "  .abc", globals::VERSIONTAG));

    args::Group grp_commands(parser, "Commands:");
    args::Command cmd_autofix(grp_commands, "autofix", "Auto-fix issues", cmd::parse::autofix);
    args::Command cmd_convert(grp_commands, "convert", "Convert file type", cmd::parse::convert);
    args::Command cmd_decompose(grp_commands, "decompose", "Decompose into individual curves", cmd::parse::decompose);
    args::Command cmd_filter(grp_commands, "filter", "Extract strands that pass given filter", cmd::parse::filter);
    args::Command cmd_findpenet(grp_commands, "findpenet", "Find penetration against head mesh", cmd::parse::findpenet);
    args::Command cmd_info(grp_commands, "info", "Print information", cmd::parse::info);
    args::Command cmd_resample(grp_commands, "resample", "Resample strands s.t. every segment is shorter than twice the shortest segment", cmd::parse::resample);
    args::Command cmd_stats(grp_commands, "stats", "Generate statistics", cmd::parse::stats);
    args::Command cmd_subsample(grp_commands, "subsample", "Subsample strands", cmd::parse::subsample);
    args::Command cmd_transform(grp_commands, "transform", "Transform strand points", cmd::parse::transform);

    args::Group grp_globals("Common options:");
    args::ValueFlag<std::string> globals_input_file(grp_globals, "PATH", "(REQUIRED) Input file", {'i', "input-file"}, args::Options::Required);
    args::ValueFlag<std::string> globals_output_ext(grp_globals, "EXT", "Output file extension", {'o', "output-ext"}, "");
    args::Flag globals_overwrite(grp_globals, "overwrite", "Overwrite when output file exists", {"overwrite"});
    args::ValueFlag<unsigned int> globals_ply_load_default_nsegs(grp_globals, "N", "Default number of segments per strand for PLY files [0]", {"ply-load-default-nsegs"}, 0);
    args::Flag globals_ply_save_ascii(grp_globals, "ply-save-ascii", "Save PLY files in ASCII format", {"ply-save-ascii"});
    args::ValueFlag<std::string> globals_verbosity(grp_globals, "NAME", "Verbosity level name {trace,debug,info,warn,error,critical,off} [info]", {'v', "verbosity"}, "info");
    args::ValueFlag<int> globals_seed(grp_globals, "N", "Seed for random number generator (-1 for time-based seed) [-1]", {"seed"}, -1);
    args::Flag globals_no_autofix(grp_globals, "no-autofix", "Do not auto-fix issues in input", {"no-autofix"});
    args::HelpFlag globals_help(grp_globals, "help", "Show this help message", {'h', "help"});

    args::GlobalOptions global_options(parser, grp_globals);

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cout << parser;
        return 0;
    }
    catch (args::Error& e)
    {
        std::cerr << e.what() << std::endl << parser;
        return 1;
    }
    catch (const std::exception &e)
    {
        spdlog::error("{}", e.what());
        return 1;
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
    globals::ply_save_ascii = globals_ply_save_ascii;

    // Seed the random number generator
    int seed = *globals_seed;
    if (seed < 0) {
        seed = std::time(nullptr);
        spdlog::info("Using time-based seed: {}", seed);
    }
    globals::rng.seed(seed);

    if (globals::cmd_wo_output.count(globals::cmd_exec)) {
        if (globals::output_ext != "") {
            spdlog::warn("Ignoring --output-ext");
            globals::output_ext = "";
        }
    } else if (globals::output_ext == "") {
        spdlog::error("You must specify output file extension by --output-ext");
        return 1;
    }

    // Get file extension from globals::input_file, in lowercase
    globals::input_ext = globals::input_file.substr(globals::input_file.find_last_of(".") + 1);
    std::transform(globals::input_ext.begin(), globals::input_ext.end(), globals::input_ext.begin(), [](unsigned char c){ return std::tolower(c); });

    // Check if input file extension is supported
    if (globals::supported_ext.count(globals::input_ext) == 0) {
        spdlog::error("Unsupported input file extension: {}", globals::input_ext);
        return 1;
    }
    globals::load_func = globals::supported_ext.at(globals::input_ext).first;

    if (globals::cmd_exec == cmd::exec::autofix) {
        if (globals_no_autofix)
            spdlog::warn("Ignoring --no-autofix");
        globals::output_ext = globals::input_ext;
    }

    // Check if output file extension is supported
    if (globals::output_ext != "") {
        if (globals::supported_ext.count(globals::output_ext) == 0) {
            spdlog::error("Unsupported output file extension: {}", globals::output_ext);
            return 1;
        }
        globals::save_func = globals::supported_ext.at(globals::output_ext).second;
    }

    // Get input file name without extension
    globals::input_file_wo_ext = globals::input_file.substr(0, globals::input_file.find_last_of("."));

    // Check if the output file already exists
    if (!globals::overwrite && globals::output_file && std::filesystem::exists(globals::output_file())) {
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
        auto hairfile_in = globals::load_func(globals::input_file);

        // Auto-fix issues in input
        if (!globals_no_autofix && globals::cmd_exec != cmd::exec::autofix) {
            auto hairfile_fixed = cmd::exec::autofix(hairfile_in);
            if (hairfile_fixed)
                hairfile_in = hairfile_fixed;
        }

        spdlog::info("Number of strands: {}", hairfile_in->GetHeader().hair_count);
        spdlog::info("Number of points: {}", hairfile_in->GetHeader().point_count);

        auto hairfile_out = globals::cmd_exec(hairfile_in);

        if (hairfile_out) {
            spdlog::info("Saving to {} ...", globals::output_file());
            globals::save_func(globals::output_file(), hairfile_out);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("{}", e.what());
        return 1;
    }

    spdlog::info("Done");
    return 0;
}
