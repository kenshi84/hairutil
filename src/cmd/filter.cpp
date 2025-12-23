#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {

struct {
    std::string& key = cmd::param::s("filter", "key");
    std::optional<float>& lt = cmd::param::opt_f("filter", "lt");
    std::optional<float>& gt = cmd::param::opt_f("filter", "gt");
    std::optional<float>& leq = cmd::param::opt_f("filter", "leq");
    std::optional<float>& geq = cmd::param::opt_f("filter", "geq");
    bool& output_indices = cmd::param::b("filter", "output_indices");
    bool& no_output = cmd::param::b("filter", "no_output");
} param;

const std::set<std::string> keys_set = {
    "length",
    "nsegs",
    "tasum",
    "maxseglength",
    "minseglength",
    "maxsegtadiff",
    "minsegtadiff",
    "maxptcrr",
    "minptcrr",
    "maxptta",
    "minptta",
    "maxptcurv",
    "minptcurv"
};

}

void cmd::parse::filter(args::Subparser &parser) {
    args::ValueFlag<std::string> key(parser, "KEY",
        "Filtering key chosen from:\n"
        "  length (Total length)\n"
        "  nsegs (Number of segments)\n"
        "  tasum (Turning angle sum)\n"
        "  maxseglength (Maximum of segment length)\n"
        "  minseglength (Minimum of segment length)\n"
        "  maxsegtadiff (Maximum of segment turning angle difference)\n"
        "  minsegtadiff (Minimum of segment turning angle difference)\n"
        "  maxptcrr (Maximum of point circumradius reciprocal)\n"
        "  minptcrr (Minimum of point circumradius reciprocal)\n"
        "  maxptta (Maximum of point turning angle)\n"
        "  minptta (Minimum of point turning angle)\n"
        "  maxptcurv (Maximum of point curvature)\n"
        "  minptcurv (Minimum of point curvature)\n"
        , {"key", 'k'}, args::Options::Required);
    args::ValueFlag<float> lt(parser, "R", "Less-than threshold", {"lt"});
    args::ValueFlag<float> gt(parser, "R", "Greater-than threshold", {"gt"});
    args::ValueFlag<float> leq(parser, "R", "Less-than or equal-to threshold", {"leq"});
    args::ValueFlag<float> geq(parser, "R", "Greater-than or equal-to threshold", {"geq"});
    args::Flag output_indices(parser, "output-indices", "Output selected strand indices as txt", {"output-indices"});
    args::Flag no_output(parser, "no-output", "Do not output filtered hair file, only show number of filtered strands", {"no-output"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::filter;
    globals::check_error = [](){
        if (::keys_set.count(::param.key) == 0) {
            throw std::runtime_error(fmt::format("Invalid key: {}", ::param.key));
        }
        if (::param.lt && ::param.leq) {
            throw std::runtime_error("Cannot specify both --lt and --leq");
        }
        if (::param.gt && ::param.geq) {
            throw std::runtime_error("Cannot specify both --gt and --geq");
        }
        if (!::param.lt && !::param.gt && !::param.leq && !::param.geq) {
            throw std::runtime_error("Must specify one of --lt, --gt, --leq, or --geq");
        }
    };
    if (!no_output) {
        globals::output_file_wo_ext = [](){
            std::string suffix;
            if (::param.gt) suffix += fmt::format("_gt_{}", *::param.gt);
            if (::param.geq) suffix += fmt::format("_geq_{}", *::param.geq);
            if (::param.lt) suffix += fmt::format("_lt_{}", *::param.lt);
            if (::param.leq) suffix += fmt::format("_leq_{}", *::param.leq);
            return fmt::format("{}_filtered_{}{}", globals::input_file_wo_ext, ::param.key, suffix);
        };
    }

    ::param.key = *key;
    ::param.lt = lt ? std::optional<float>(*lt) : std::nullopt;
    ::param.gt = gt ? std::optional<float>(*gt) : std::nullopt;
    ::param.leq = leq ? std::optional<float>(*leq) : std::nullopt;
    ::param.geq = geq ? std::optional<float>(*geq) : std::nullopt;
    ::param.output_indices = output_indices;
    ::param.no_output = no_output;
}

std::shared_ptr<cyHairFile> cmd::exec::filter(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    // Flag for whether a strand is selected
    std::vector<unsigned char> selected(header_in.hair_count, 0);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < header_in.hair_count; ++i) {
        const unsigned int nsegs = header_in.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments;

        float strand_length = 0.0f;
        float turning_angle_sum = 0.0f;
        float max_segment_length = 0.0f;
        float min_segment_length = std::numeric_limits<float>::max();
        float max_segment_turning_angle_difference = 0.0f;
        float min_segment_turning_angle_difference = std::numeric_limits<float>::max();
        float max_point_circumradius_reciprocal = 0.0f;
        float min_point_circumradius_reciprocal = std::numeric_limits<float>::max();
        float max_point_turning_angle = 0.0f;
        float min_point_turning_angle = std::numeric_limits<float>::max();
        float max_point_curvature = 0.0f;
        float min_point_curvature = std::numeric_limits<float>::max();

        Vector3f prev_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * offset);
        float prev_turning_angle;
        for (unsigned int j = 0; j < nsegs; ++j) {
            const Vector3f point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
            const float segment_length = (point - prev_point).norm();

            max_segment_length = std::max(max_segment_length, segment_length);
            min_segment_length = std::min(min_segment_length, segment_length);

            if (j < nsegs - 1) {
                // Compute the circumradius of the wedge triangle
                const Vector3f next_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 2));
                const float la = (point - prev_point).norm();
                const float lb = (next_point - point).norm();
                const float lc = (next_point - prev_point).norm();
                const float s = (la + lb + lc) / 2.0f;
                const float A = std::sqrt(s * (s - la) * (s - lb) * (s - lc));
                const float circumradius_reciprocal = A > 0.0f ? 1.0f / (la * lb * lc / (4.0f * A)) : 0.0f;
                const float turning_angle_rad = globals::pi - std::acos(std::clamp((la * la + lb * lb - lc * lc) / (2.0f * la * lb), -1.0f, 1.0f));
                const float turning_angle = turning_angle_rad * 180.0f / globals::pi;
                const float curvature = turning_angle_rad / ((la + lb) / 2.0f);

                max_point_circumradius_reciprocal = std::max(max_point_circumradius_reciprocal, circumradius_reciprocal);
                min_point_circumradius_reciprocal = std::min(min_point_circumradius_reciprocal, circumradius_reciprocal);
                max_point_turning_angle = std::max(max_point_turning_angle, turning_angle);
                min_point_turning_angle = std::min(min_point_turning_angle, turning_angle);
                max_point_curvature = std::max(max_point_curvature, curvature);
                min_point_curvature = std::min(min_point_curvature, curvature);

                turning_angle_sum += turning_angle;

                if (j > 0) {
                    const float turning_angle_difference = std::abs(turning_angle - prev_turning_angle);
                    max_segment_turning_angle_difference = std::max(max_segment_turning_angle_difference, turning_angle_difference);
                    min_segment_turning_angle_difference = std::min(min_segment_turning_angle_difference, turning_angle_difference);
                }
                prev_turning_angle = turning_angle;
            }

            strand_length += segment_length;
            prev_point = point;
        }

        offset += nsegs + 1;

        double value;
        if (::param.key == "length") value = strand_length;
        if (::param.key == "nsegs") value = nsegs;
        if (::param.key == "tasum") value = turning_angle_sum;
        if (::param.key == "maxseglength") value = max_segment_length;
        if (::param.key == "minseglength") value = min_segment_length;
        if (::param.key == "maxsegtadiff") value = max_segment_turning_angle_difference;
        if (::param.key == "minsegtadiff") value = min_segment_turning_angle_difference;
        if (::param.key == "maxptcrr") value = max_point_circumradius_reciprocal;
        if (::param.key == "minptcrr") value = min_point_circumradius_reciprocal;
        if (::param.key == "maxptta") value = max_point_turning_angle;
        if (::param.key == "minptta") value = min_point_turning_angle;
        if (::param.key == "maxptcurv") value = max_point_curvature;
        if (::param.key == "minptcurv") value = min_point_curvature;

        if (::param.lt && value >= *::param.lt) continue;
        if (::param.gt && value <= *::param.gt) continue;
        if (::param.leq && value > *::param.leq) continue;
        if (::param.geq && value < *::param.geq) continue;

        selected[i] = 1;
    }
    spdlog::info("{} strands selected", std::count(selected.begin(), selected.end(), 1));
    if (::param.output_indices) {
        std::string suffix;
        if (::param.gt) suffix += fmt::format("_gt_{}", *::param.gt);
        if (::param.geq) suffix += fmt::format("_geq_{}", *::param.geq);
        if (::param.lt) suffix += fmt::format("_lt_{}", *::param.lt);
        if (::param.leq) suffix += fmt::format("_leq_{}", *::param.leq);
        std::string indices_file = util::path_under_optional_dir(fmt::format("{}_filtered_{}{}_indices.txt", globals::input_file_wo_ext, ::param.key, suffix), globals::output_dir);
        if (!globals::overwrite && std::filesystem::exists(indices_file)) {
            throw std::runtime_error("File already exists: " + indices_file + ". Use --overwrite to overwrite.");
        }
        std::ofstream ofs(indices_file);
        if (!ofs) {
            throw std::runtime_error(fmt::format("Failed to open file: {}", indices_file));
        }
        for (unsigned int i = 0; i < selected.size(); ++i) {
            if (selected[i]) {
                ofs << i << "\n";
            }
        }
        spdlog::info("Selected strand indices written to {}", indices_file);
    }

    if (::param.no_output)
        return {};
    return util::get_subset(hairfile_in, selected);
}
