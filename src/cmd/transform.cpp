#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {
float s;
Vector3f t;
Matrix3f R;
}

void cmd::parse::transform(args::Subparser &parser) {
    args::ValueFlag<float> scale(parser, "R", "Uniform scaling factor [1.0]", {"scale", 's'}, 1.0f);
    args::ValueFlag<std::string> translate(parser, "R,R,R", "Comma-separated 3D vector for translation [0,0,0]", {"translate", 't'}, "0,0,0");
    args::ValueFlag<std::string> rotate(parser, "R,R,R,R,R,R,R,R,R", "Comma-separated row-major 3x3 matrix for rotation [1,0,0,0,1,0,0,0,1]", {"rotate", 'R'}, "1,0,0,0,1,0,0,0,1");
    parser.Parse();
    globals::cmd_exec = cmd::exec::transform;
    globals::output_file = [](){
        std::string t_str = fmt::format("{}", ::t.transpose());
        t_str = util::squash_underscores(util::replace_space_with_underscore(util::trim_whitespaces(t_str)));

        const Matrix3f Rt = ::R.transpose();
        std::string R_str = fmt::format("{}", Map<const Matrix<float, 1, 9>>(Rt.data()));
        R_str = util::squash_underscores(util::replace_space_with_underscore(util::trim_whitespaces(R_str)));

        return fmt::format("{}_tfm_s_{}_t_{}_R_{}.{}", globals::input_file_wo_ext, ::s, t_str, R_str, globals::output_ext);
    };

    ::s = *scale;
    const std::vector<float> translate_vec = util::parse_comma_separated_values<float>(*translate);
    if (translate_vec.size() != 3) {
        throw std::runtime_error(fmt::format("Invalid translation vector: {}", *translate));
    }
    ::t = Map<const Vector3f>(translate_vec.data());
    const std::vector<float> rotate_vec = util::parse_comma_separated_values<float>(*rotate);
    if (rotate_vec.size() != 9) {
        throw std::runtime_error(fmt::format("Invalid rotation matrix: {}", *rotate));
    }
    ::R = Map<const Matrix3f>(rotate_vec.data()).transpose();
}

std::shared_ptr<cyHairFile> cmd::exec::transform(std::shared_ptr<cyHairFile> hairfile_in) {
    unsigned int offset = 0;
    for (unsigned int i = 0; i < hairfile_in->GetHeader().hair_count; ++i) {
        const unsigned int segment_count = (hairfile_in->GetHeader().arrays & _CY_HAIR_FILE_SEGMENTS_BIT) ? hairfile_in->GetSegmentsArray()[i] : hairfile_in->GetHeader().d_segments;

        for (unsigned int j = 0; j < segment_count + 1; ++j) {
            Map<Vector3f> point(hairfile_in->GetPointsArray() + 3 * (offset + j));
            const Vector3f point_new = ::s * ::R * point + ::t;
            point = point_new;
        }
        offset += segment_count + 1;
    }

    return hairfile_in;
}
