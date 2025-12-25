#include "cmd.h"
#include "util.h"

#include <happly.h>

using namespace Eigen;

namespace {
struct {
    float& radius = cmd::param::f("tubify", "radius");
    unsigned int& num_sides = cmd::param::ui("tubify", "num_sides");
    bool& capped = cmd::param::b("tubify", "capped");
    bool& colored = cmd::param::b("tubify", "colored");
} param;
}

void cmd::parse::tubify(args::Subparser &parser)
{
    args::ValueFlag<float> radius(parser, "R", "(REQUIRED) Tube radius", {"radius", 'r'}, args::Options::Required);
    args::ValueFlag<unsigned int> num_sides(parser, "N", "Number of sides of tubes [6]", {"num-sides", 'n'}, 6);
    args::Flag capped(parser, "", "Cap tube ends", {"capped"});
    args::Flag colored(parser, "", "Output colored vertices", {"colored"});
    parser.Parse();
    globals::cmd_exec = &cmd::exec::tubify;
    globals::check_error = []() {
        const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_tube.ply", globals::output_dir);
        if (!globals::overwrite && std::filesystem::exists(output_file)) {
            throw std::runtime_error("File already exists: " + output_file + ". Use --overwrite to overwrite.");
        }
    };
    ::param.radius = *radius;
    ::param.num_sides = *num_sides;
    ::param.capped = capped;
    ::param.colored = colored;
}

std::shared_ptr<cyHairFile> cmd::exec::tubify(std::shared_ptr<cyHairFile> hairfile) {
    const auto& header = hairfile->GetHeader();

    std::vector<size_t> segments_array(header.hair_count, header.d_segments);
    if (header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) {
        for (unsigned int i = 0; i < header.hair_count; ++i)
        segments_array[i] = hairfile->GetSegmentsArray()[i];
    }

    const size_t total_num_vertices = header.point_count * ::param.num_sides;
    const size_t total_num_segments = std::accumulate(segments_array.begin(), segments_array.end(), 0);
    const size_t total_num_faces = ::param.num_sides * 2 * total_num_segments + (::param.capped ? 2 * header.hair_count : 0);

    std::vector<std::array<double, 3>> vertex_xyz;
    std::vector<std::array<double, 3>> vertex_rgb;
    std::vector<std::vector<uint32_t>> faces;
    vertex_xyz.reserve(total_num_vertices);
    vertex_rgb.reserve(total_num_vertices);
    faces.reserve(total_num_faces);
    size_t offset = 0;
    std::uniform_real_distribution<float> dist(0, 1);
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const Vector3f random_color(dist(globals::rng), dist(globals::rng), dist(globals::rng));
        const size_t num_segments = segments_array[i];
        Vector3f tangent;
        for (size_t j = 0; j <= num_segments; ++j) {
            const Vector3f center(&hairfile->GetPointsArray()[3 * (offset + j)]);
            if (j < num_segments) {
                tangent = (Vector3f(&hairfile->GetPointsArray()[3 * (offset + j + 1)]) - center).normalized();
            }
            Vector3f normal = tangent.tail(2).isZero() ? Vector3f::UnitY() : Vector3f::UnitX();
            normal = (normal - tangent * normal.dot(tangent)).normalized();
            const Vector3f binormal = tangent.cross(normal);
            for (unsigned int k = 0; k < ::param.num_sides; ++k) {
                const float theta = k * 2 * std::numbers::pi / ::param.num_sides;
                const Vector3f pos = center + ::param.radius * (std::cos(theta) * normal + std::sin(theta) * binormal);
                const Vector3f color = (header.arrays & _CY_HAIR_FILE_COLORS_BIT) ? Vector3f(&hairfile->GetColorsArray()[3 * (offset + j)]) : random_color;
                vertex_xyz.push_back({});
                vertex_rgb.push_back({});
                util::copy_vec3(pos, vertex_xyz.back());
                util::copy_vec3(color, vertex_rgb.back());
                if (j < num_segments) {
                    const uint32_t fv0 = ::param.num_sides * (offset + j) + k;
                    const uint32_t fv1 = ::param.num_sides * (offset + j) + (k + 1) % ::param.num_sides;
                    const uint32_t fv2 = ::param.num_sides * (offset + j + 1) + (k + 1) % ::param.num_sides;
                    const uint32_t fv3 = ::param.num_sides * (offset + j + 1) + k;
                    faces.push_back({fv0, fv1, fv2});
                    faces.push_back({fv2, fv3, fv0});
                }
            }
        }
        if (::param.capped) {
            std::vector<uint32_t> cap_head(::param.num_sides);
            std::vector<uint32_t> cap_tail(::param.num_sides);
            for (unsigned int k = 0; k < ::param.num_sides; ++k) {
                cap_head[k] = ::param.num_sides * offset + k;
                cap_tail[k] = ::param.num_sides * (offset + num_segments) + k;
            }
            std::reverse(cap_head.begin(), cap_head.end());
            faces.push_back(cap_head);
            faces.push_back(cap_tail);
        }
        offset += num_segments + 1;
    }

    happly::PLYData ply;
    ply.addVertexPositions(vertex_xyz);
    if (::param.colored)
        ply.addVertexColors(vertex_rgb);
    ply.addFaceIndices(faces);
    const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_tube.ply", globals::output_dir);
    ply.write(output_file, globals::ply_save_ascii ? happly::DataFormat::ASCII : happly::DataFormat::Binary);
    log_info("Written to {}", output_file);
    return {};
}
