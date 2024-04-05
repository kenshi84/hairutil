#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {
struct {
std::string s;
std::string t;
std::string r;
std::string f;
Matrix4f M = Matrix4f::Identity();
} param;
}

void cmd::parse::transform(args::Subparser &parser) {
    args::ValueFlag<std::string> scale(parser, "R or R,R,R", "Scaling factor; either a single number of comma-separated 3-tuple for non-uniform scaling", {"scale", 's'}, "");
    args::ValueFlag<std::string> translate(parser, "R,R,R", "Comma-separated 3-vector for translation", {"translate", 't'}, "");
    args::ValueFlag<std::string> rotate(parser, "R,R,R,R,R,R,R,R,R", "Comma-separated row-major 3x3 matrix for rotation", {"rotate", 'r'}, "");
    args::ValueFlag<std::string> full(parser, "R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R", "Comma-separated row-major 4x4 matrix for full transform", {"full", 'f'}, "");
    parser.Parse();
    globals::cmd_exec = cmd::exec::transform;
    globals::output_file = [](){
        if (!param.s.empty())
            return fmt::format("{}_tfm_s_{}.{}", globals::input_file_wo_ext, param.s, globals::output_ext);
        if (!param.t.empty())
            return fmt::format("{}_tfm_t_{}.{}", globals::input_file_wo_ext, param.t, globals::output_ext);
        if (!param.r.empty())
            return fmt::format("{}_tfm_r_{}.{}", globals::input_file_wo_ext, param.r, globals::output_ext);
        return fmt::format("{}_tfm_f_{}.{}", globals::input_file_wo_ext, param.f, globals::output_ext);
    };
    globals::check_error = [](){
        if (param.s.empty() + param.t.empty() + param.r.empty() + param.f.empty() != 3) {
            throw std::runtime_error("Exactly one of --scale, --translate, --rotate, or --full must be specified");
        }
    };

    ::param = {};
    ::param.s = *scale;
    ::param.t = *translate;
    ::param.r = *rotate;
    ::param.f = *full;

    if (!::param.s.empty()) {
        const std::vector<float> scale_vec = util::parse_comma_separated_values<float>(*scale);
        if (scale_vec.size() == 1) {
            ::param.M.block<3, 3>(0, 0) = scale_vec[0] * Matrix3f::Identity();
        } else if (scale_vec.size() == 3) {
            ::param.M.block<3, 3>(0, 0) = Map<const Vector3f>(scale_vec.data()).asDiagonal();
        } else {
            throw std::runtime_error(fmt::format("Invalid scaling factor: {}", *scale));
        }
    }

    if (!::param.t.empty()) {
        const std::vector<float> translate_vec = util::parse_comma_separated_values<float>(*translate);
        if (translate_vec.size() != 3) {
            throw std::runtime_error(fmt::format("Invalid translation vector: {}", *translate));
        }
        ::param.M.block<3, 1>(0, 3) = Map<const Vector3f>(translate_vec.data());
    }

    if (!::param.r.empty()) {
        const std::vector<float> rotate_vec = util::parse_comma_separated_values<float>(*rotate);
        if (rotate_vec.size() != 9) {
            throw std::runtime_error(fmt::format("Invalid rotation matrix: {}", *rotate));
        }
        ::param.M.block<3, 3>(0, 0) = Map<const Matrix3f>(rotate_vec.data()).transpose();
    }

    if (!::param.f.empty()) {
        const std::vector<float> full_vec = util::parse_comma_separated_values<float>(*full);
        if (full_vec.size() != 16) {
            throw std::runtime_error(fmt::format("Invalid transformation matrix: {}", *full));
        }
        ::param.M = Map<const Matrix4f>(full_vec.data()).transpose();
    }
}

std::shared_ptr<cyHairFile> cmd::exec::transform(std::shared_ptr<cyHairFile> hairfile_in) {
    unsigned int offset = 0;
    for (unsigned int i = 0; i < hairfile_in->GetHeader().hair_count; ++i) {
        const unsigned int segment_count = (hairfile_in->GetHeader().arrays & _CY_HAIR_FILE_SEGMENTS_BIT) ? hairfile_in->GetSegmentsArray()[i] : hairfile_in->GetHeader().d_segments;

        for (unsigned int j = 0; j < segment_count + 1; ++j) {
            Map<Vector3f> point(hairfile_in->GetPointsArray() + 3 * (offset + j));
            const Vector4f hpoint = (Vector4f() << point, 1).finished();
            const Vector4f hpoint_new = ::param.M * hpoint;
            point = hpoint_new.head(3) / hpoint_new(3);
        }
        offset += segment_count + 1;
    }

    return hairfile_in;
}
