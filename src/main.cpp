#include <ctime>

#include "cmd.h"
#include "io.h"
#include "util.h"

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
        "  .abc\n"
        "  .npy", globals::VERSIONTAG));
    parser.helpParams.width = 120;
    parser.helpParams.helpindent = 32;

    args::Group grp_commands(parser, "Commands:");
    args::Command cmd_autofix(grp_commands, "autofix", "Auto-fix issues", cmd::parse::autofix);
    args::Command cmd_convert(grp_commands, "convert", "Convert file type", cmd::parse::convert);
    args::Command cmd_decompose(grp_commands, "decompose", "Decompose into individual curves", cmd::parse::decompose);
    args::Command cmd_filter(grp_commands, "filter", "Extract strands that pass given filter", cmd::parse::filter);
    args::Command cmd_findpenet(grp_commands, "findpenet", "Find penetration against head mesh", cmd::parse::findpenet);
    args::Command cmd_getcurvature(grp_commands, "getcurvature", "Get discrete curvature & torsion", cmd::parse::getcurvature);
    args::Command cmd_info(grp_commands, "info", "Print information", cmd::parse::info);
    args::Command cmd_resample(grp_commands, "resample", "Resample strands s.t. every segment is shorter than twice the target segment length", cmd::parse::resample);
    args::Command cmd_smooth(grp_commands, "smooth", "Smooth strands", cmd::parse::smooth);
    args::Command cmd_stats(grp_commands, "stats", "Generate statistics", cmd::parse::stats);
    args::Command cmd_subsample(grp_commands, "subsample", "Subsample strands", cmd::parse::subsample);
    args::Command cmd_transform(grp_commands, "transform", "Transform strand points, either by one of scale/translate/rotate, or by full 4x4 matrix", cmd::parse::transform);
    args::Command cmd_tubify(grp_commands, "tubify", "Turn curves into tubes as triangle mesh", cmd::parse::tubify);

    args::Group grp_globals("Common options:");
    args::ValueFlag<std::string> globals_input_file(grp_globals, "PATH", "(REQUIRED) Input file", {'i', "input-file"}, args::Options::Required);
    args::ValueFlag<std::string> globals_output_ext(grp_globals, "EXT", "Output file extension (or extensions by comma-delimited list); when omitted, use input file extension", {'o', "output-ext"}, "");
    args::Flag globals_overwrite(grp_globals, "overwrite", "Overwrite when output file exists", {"overwrite"});
    args::ValueFlag<std::string> globals_output_dir(grp_globals, "DIR", "Output directory; if not specified, same as the input file", {'d', "output-dir"}, "");
    args::ValueFlag<unsigned int> globals_ply_load_default_nsegs(grp_globals, "N", "Default number of segments per strand for PLY files [0]", {"ply-load-default-nsegs"}, 0);
    args::Flag globals_ply_save_ascii(grp_globals, "ply-save-ascii", "Save PLY files in ASCII format", {"ply-save-ascii"});
    args::ValueFlag<std::string> globals_verbosity(grp_globals, "NAME", "Verbosity level name {trace,debug,info,warn,error,critical,off} [info]", {'v', "verbosity"}, "info");
    args::Flag globals_print_json(grp_globals, "print-json", "Print log messages in JSON format, disabling standard logging", {'j', "print-json"});
    args::ValueFlag<int> globals_seed(grp_globals, "N", "Seed for random number generator (-1 for time-based seed) [0]", {"seed"}, 0);
    args::Flag globals_no_autofix(grp_globals, "no-autofix", "Do not auto-fix issues in input", {"no-autofix"});
    args::HelpFlag globals_help(grp_globals, "help", "Show this help message", {'h', "help"});

    args::GlobalOptions global_options(parser, grp_globals);

    globals::json = {
        {"version", globals::VERSIONTAG},
        {"log", {
            {"debug", nlohmann::json::array()},
            {"info", nlohmann::json::array()},
            {"warn", nlohmann::json::array()},
            {"error", nlohmann::json::array()},
            {"critical", nlohmann::json::array()}
        }},
    };

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
        log_error("{}", e.what());
        return 1;
    }

    if (globals_print_json) {
        if (*globals_verbosity != "off") {
            globals::json["warnings"].push_back(fmt::format("Ignoring --verbosity={}, as --print-json is specified", *globals_verbosity));
            *globals_verbosity = "off";
        }
    }
    auto scope_guard = sg::make_scope_guard([&]{
        if (globals_print_json)
            cout << globals::json.dump(2) << std::endl;
    });
    spdlog::set_level(
        *globals_verbosity == "trace" ? spdlog::level::trace :
        *globals_verbosity == "debug" ? spdlog::level::debug :
        *globals_verbosity == "info" ? spdlog::level::info :
        *globals_verbosity == "warn" ? spdlog::level::warn :
        *globals_verbosity == "error" ? spdlog::level::err :
        *globals_verbosity == "critical" ? spdlog::level::critical : spdlog::level::off
    );

    globals::input_file = *globals_input_file;
    globals::output_exts = util::container_cast<std::set<std::string>>(util::parse_comma_separated_values<std::string>(*globals_output_ext));
    globals::output_dir = *globals_output_dir;
    globals::overwrite = globals_overwrite;
    globals::ply_load_default_nsegs = *globals_ply_load_default_nsegs;
    globals::ply_save_ascii = globals_ply_save_ascii;

    // Seed the random number generator
    int seed = *globals_seed;
    if (seed < 0) {
        seed = std::time(nullptr);
        log_info("Using time-based seed: {}", seed);
    }
    globals::rng.seed(seed);

    // Get file extension from globals::input_file, in lowercase
    globals::input_ext = globals::input_file.substr(globals::input_file.find_last_of(".") + 1);
    std::transform(globals::input_ext.begin(), globals::input_ext.end(), globals::input_ext.begin(), [](unsigned char c){ return std::tolower(c); });

    if (!globals::output_file_wo_ext) {
        if (!globals::output_exts.empty()) {
            log_warn("Ignoring --output-ext");
            globals::output_exts = {};
        }
    } else if (globals::output_exts.empty()) {
        log_warn("--output-ext not specified, using input file extension: {}", globals::input_ext);
        globals::output_exts.insert(globals::input_ext);
    }

    if (globals::output_dir != "") {
        std::filesystem::path output_dir = globals::output_dir;
        if (std::filesystem::exists(output_dir)) {
            if (!std::filesystem::is_directory(output_dir)) {
                log_error("{} is not a directory", output_dir.string());
                return 1;
            }
        } else {
            if (!std::filesystem::create_directories(output_dir)) {
                log_error("Failed to create directory: {}", output_dir.string());
                return 1;
            }
        }
        if (globals::output_file_wo_ext) {
            globals::output_file_wo_ext.dir = output_dir.string();
        }
    }

    // Check if input file extension is supported
    if (globals::supported_ext.count(globals::input_ext) == 0) {
        log_error("Unsupported input file extension: {}", globals::input_ext);
        return 1;
    }
    const io::load_func_t load_func = globals::supported_ext.at(globals::input_ext).first;

    if (globals::cmd_exec == cmd::exec::autofix) {
        if (globals_no_autofix)
            log_warn("Ignoring --no-autofix");
    }

    // Check if output file extension is supported
    for (const std::string& output_ext : globals::output_exts) {
        if (globals::supported_ext.count(output_ext) == 0) {
            log_error("Unsupported output file extension: {}", output_ext);
            return 1;
        }
    }

    // Get input file name without extension
    globals::input_file_wo_ext = globals::input_file.substr(0, globals::input_file.find_last_of("."));

    // Check output filename validity and existence
    std::unordered_map<std::string, std::string> output_files;
    if (globals::output_file_wo_ext) {
        for (const std::string& output_ext : globals::output_exts) {
            output_files[output_ext] = globals::output_file_wo_ext() + "." + output_ext;

            // Truncate if too long
            if (output_files[output_ext].length() > 230) {
                log_warn("Output file path is too long ({}), truncating", output_files[output_ext].length());
                output_files[output_ext] = output_files[output_ext].substr(0, 230) + "." + output_ext;
            }

            if (!globals::overwrite && std::filesystem::exists(output_files[output_ext])) {
                log_error("Output file already exists: {}", output_files[output_ext]);
                log_error("Use --overwrite to overwrite the file");
                return 1;
            }
        }
    }

    try
    {
        // Perform precursory checks, if any
        if (globals::check_error)
            globals::check_error();

        log_info("Loading from {} ...", globals::input_file);
        globals::json["input"]["file"] = globals::input_file;
        auto hairfile_in = load_func(globals::input_file);

        // Auto-fix issues in input
        if (!globals_no_autofix && globals::cmd_exec != cmd::exec::autofix) {
            auto hairfile_fixed = cmd::exec::autofix(hairfile_in);
            if (hairfile_fixed)
                hairfile_in = hairfile_fixed;
        }

        log_info("Number of strands: {}", hairfile_in->GetHeader().hair_count);
        log_info("Number of points: {}", hairfile_in->GetHeader().point_count);
        globals::json["input"]["num_strands"] = hairfile_in->GetHeader().hair_count;
        globals::json["input"]["num_points"] = hairfile_in->GetHeader().point_count;

        auto hairfile_out = globals::cmd_exec(hairfile_in);

        if (hairfile_out) {
            globals::json["output"]["file"] = nlohmann::json::array();
            globals::json["output"]["num_strands"] = hairfile_out->GetHeader().hair_count;
            globals::json["output"]["num_points"] = hairfile_out->GetHeader().point_count;
            for (const auto& [output_ext, output_file] : output_files) {
                const auto save_func = globals::supported_ext.at(output_ext).second;
                log_info("Saving to {} ...", output_file);
                globals::json["output"]["file"].push_back(output_file);
                save_func(output_file, hairfile_out);
            }
        }
    }
    catch (const std::exception &e)
    {
        log_error("{}", e.what());
        return 1;
    }

    log_info("Done");
    return 0;
}
