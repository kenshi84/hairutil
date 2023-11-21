#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {
std::string key;
std::optional<float> lt;
std::optional<float> gt;
std::optional<float> leq;
std::optional<float> geq;
const std::set<std::string> keys_set = {
    "length",
    "nsegs",
    "tasum",
    "maxseglength",
    "minseglength",
    "maxptcrr",
    "minptcrr",
    "maxptta",
    "minptta"
};
}

void cmd::parse::filter(args::Subparser &parser) {
    args::ValueFlag<std::string> key(parser, "KEY",
        "Filtering key chosen from:\n"
        "length (Total length)\n"
        "nsegs (Number of segments)\n"
        "tasum (Turning angle sum)\n"
        "maxseglength (Maximum of segment length)\n"
        "minseglength (Minimum of segment length)\n"
        "maxptcrr (Maximum of point circumradius reciprocal)\n"
        "minptcrr (Minimum of point circumradius reciprocal)\n"
        "maxptta (Maximum of point turning angle)\n"
        "minptta (Minimum of point turning angle)\n"
        , {"key", 'k'}, args::Options::Required);
    args::ValueFlag<float> lt(parser, "R", "Less-than threshold", {"lt"});
    args::ValueFlag<float> gt(parser, "R", "Greater-than threshold", {"gt"});
    args::ValueFlag<float> leq(parser, "R", "Less-than or equal-to threshold", {"leq"});
    args::ValueFlag<float> geq(parser, "R", "Greater-than or equal-to threshold", {"geq"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::filter;
    globals::check_error = [](){
        if (::keys_set.count(::key) == 0) {
            throw std::runtime_error(fmt::format("Invalid key: {}", ::key));
        }
        if (::lt && ::leq) {
            throw std::runtime_error("Cannot specify both --lt and --leq");
        }
        if (::gt && ::geq) {
            throw std::runtime_error("Cannot specify both --gt and --geq");
        }
        if (!::lt && !::gt && !::leq && !::geq) {
            throw std::runtime_error("Must specify one of --lt, --gt, --leq, or --geq");
        }
    };
    globals::output_file = [](){
        std::string suffix;
        if (::gt) suffix += fmt::format("_gt_{}", *::gt);
        if (::geq) suffix += fmt::format("_geq_{}", *::geq);
        if (::lt) suffix += fmt::format("_lt_{}", *::lt);
        if (::leq) suffix += fmt::format("_leq_{}", *::leq);
        return fmt::format("{}_filtered_{}{}.{}", globals::input_file_wo_ext, ::key, suffix, globals::output_ext);
    };

    ::key = *key;
    if (lt) ::lt = *lt;
    if (gt) ::gt = *gt;
    if (leq) ::leq = *leq;
    if (geq) ::geq = *geq;
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
        float max_point_circumradius_reciprocal = 0.0f;
        float min_point_circumradius_reciprocal = std::numeric_limits<float>::max();
        float max_point_turning_angle = 0.0f;
        float min_point_turning_angle = std::numeric_limits<float>::max();

        Vector3f prev_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * offset);
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
                const float turning_angle = 180.0f - std::acos(std::clamp((la * la + lb * lb - lc * lc) / (2.0f * la * lb), -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float>;

                max_point_circumradius_reciprocal = std::max(max_point_circumradius_reciprocal, circumradius_reciprocal);
                min_point_circumradius_reciprocal = std::min(min_point_circumradius_reciprocal, circumradius_reciprocal);
                max_point_turning_angle = std::max(max_point_turning_angle, turning_angle);
                min_point_turning_angle = std::min(min_point_turning_angle, turning_angle);

                turning_angle_sum += turning_angle;
            }

            strand_length += segment_length;
            prev_point = point;
        }

        offset += nsegs + 1;

        double value;
        if (::key == "length") value = strand_length;
        if (::key == "nsegs") value = nsegs;
        if (::key == "tasum") value = turning_angle_sum;
        if (::key == "maxseglength") value = max_segment_length;
        if (::key == "minseglength") value = min_segment_length;
        if (::key == "maxptcrr") value = max_point_circumradius_reciprocal;
        if (::key == "minptcrr") value = min_point_circumradius_reciprocal;
        if (::key == "maxptta") value = max_point_turning_angle;
        if (::key == "minptta") value = min_point_turning_angle;

        if (::lt && value >= *::lt) continue;
        if (::gt && value <= *::gt) continue;
        if (::leq && value > *::leq) continue;
        if (::geq && value < *::geq) continue;

        selected[i] = 1;
    }

    return util::get_subset(hairfile_in, selected);
}
