#include "cmd.h"

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

        if (has_segments) {
            out_segments.push_back(num_segments);
        }

        for (unsigned int j = 0; j <= num_segments; ++j) {
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

    // Copy data to arrays of hairfile_out
    std::memcpy(hairfile_out->GetPointsArray(), out_points.data(), out_points.size() * sizeof(float));
    if (has_segments) std::memcpy(hairfile_out->GetSegmentsArray(), out_segments.data(), out_segments.size() * sizeof(unsigned short));
    if (has_thickness) std::memcpy(hairfile_out->GetThicknessArray(), out_thickness.data(), out_thickness.size() * sizeof(float));
    if (has_transparency) std::memcpy(hairfile_out->GetTransparencyArray(), out_transparency.data(), out_transparency.size() * sizeof(float));
    if (has_color) std::memcpy(hairfile_out->GetColorsArray(), out_color.data(), out_color.size() * sizeof(float));

    return hairfile_out;
}
