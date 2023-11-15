#include "cmd.h"

using namespace Eigen;

void cmd::parse::autofix(args::Subparser &parser) {
    parser.Parse();
    globals::cmd_exec = cmd::exec::autofix;
    globals::output_file = [](){ return globals::input_file; };
}

std::shared_ptr<cyHairFile> cmd::exec::autofix(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    const bool has_segments = hairfile_in->GetSegmentsArray() != nullptr;
    const bool has_thickness = hairfile_in->GetThicknessArray() != nullptr;
    const bool has_transparency = hairfile_in->GetTransparencyArray() != nullptr;
    const bool has_color = hairfile_in->GetColorsArray() != nullptr;

    const unsigned int in_hair_count = header_in.hair_count;
    const unsigned int in_point_count = header_in.point_count;

    std::vector<unsigned short> out_segments;   out_segments.reserve(in_hair_count);
    std::vector<float> out_points;              out_points.reserve(in_point_count * 3);
    std::vector<float> out_thickness;           out_thickness.reserve(in_point_count);
    std::vector<float> out_transparency;        out_transparency.reserve(in_point_count);
    std::vector<float> out_color;               out_color.reserve(in_point_count * 3);

    bool fixed = false;

    unsigned int total_num_err_segments = 0;
    unsigned int out_hair_count = 0;
    unsigned int offset = 0;
    for (unsigned int i = 0; i < in_hair_count; ++i) {
        const unsigned int num_segments = has_segments ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments;

        if (num_segments == 0) {
            spdlog::warn("Strand {} has no segments, removed", i);
            fixed = true;
            offset += 1;
            continue;
        }

        int num_err_segments = 0;

        Vector3f prev_point;
        for (unsigned int j = 0; j <= num_segments; ++j) {
            const Vector3f point = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j));

            if (j > 0 && prev_point == point) {
                spdlog::warn("Strand {} has duplicated point at segment {}, removed", i, j);
                fixed = true;
                ++num_err_segments;
                continue;
            }
            prev_point = point;

            for (unsigned int k = 0; k < 3; ++k) {
                out_points.push_back(hairfile_in->GetPointsArray()[3*(offset + j) + k]);
                if (has_color) {
                    out_color.push_back(hairfile_in->GetColorsArray()[3*(offset + j) + k]);
                }
            }
            if (has_thickness) {
                out_thickness.push_back(hairfile_in->GetThicknessArray()[offset + j]);
            }
            if (has_transparency) {
                out_transparency.push_back(hairfile_in->GetTransparencyArray()[offset + j]);
            }
        }

        out_segments.push_back(num_segments - num_err_segments);

        total_num_err_segments += num_err_segments;
        ++out_hair_count;
        offset += num_segments + 1;
    }

    // If no issue is found, return nullptr
    if (!fixed)
        return {};

    // Create output hair file
    std::shared_ptr<cyHairFile> hairfile_out = std::make_shared<cyHairFile>();

    // Copy header via memcpy
    std::memcpy((void*)&hairfile_out->GetHeader(), &header_in, sizeof(cyHairFile::Header));

    // Set hair_count & point_count, allocate arrays
    hairfile_out->SetHairCount(out_hair_count);
    hairfile_out->SetPointCount(out_points.size() / 3);
    hairfile_out->SetArrays(header_in.arrays);

    if (total_num_err_segments > 0) {
        hairfile_out->SetArrays(header_in.arrays | _CY_HAIR_FILE_SEGMENTS_BIT);
        hairfile_out->SetDefaultSegmentCount(0);
    }

    // Copy data to arrays of hairfile_out
    std::memcpy(hairfile_out->GetPointsArray(), out_points.data(), out_points.size() * sizeof(float));

    if (has_segments || total_num_err_segments > 0) {
        std::memcpy(hairfile_out->GetSegmentsArray(), out_segments.data(), out_segments.size() * sizeof(unsigned short));
    }

    if (has_thickness) std::memcpy(hairfile_out->GetThicknessArray(), out_thickness.data(), out_thickness.size() * sizeof(float));
    if (has_transparency) std::memcpy(hairfile_out->GetTransparencyArray(), out_transparency.data(), out_transparency.size() * sizeof(float));
    if (has_color) std::memcpy(hairfile_out->GetColorsArray(), out_color.data(), out_color.size() * sizeof(float));

    return hairfile_out;
}
