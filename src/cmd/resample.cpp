#include "cmd.h"

using namespace Eigen;

namespace {
struct {
    float& target_segment_length = cmd::param::f("resample", "target_segment_length");
} param;
}

void cmd::parse::resample(args::Subparser &parser) {
    args::ValueFlag<float> target_segment_length(parser, "R", "Target segment length [2.0]", {"target-segment-length"}, 2.0f);
    parser.Parse();
    globals::cmd_exec = cmd::exec::resample;
    globals::output_file = [](){ return fmt::format("{}_resampled_tsl_{}.{}", globals::input_file_wo_ext, ::param.target_segment_length, globals::output_ext); };
    globals::check_error = [](){
        if (::param.target_segment_length <= 0) {
            throw std::runtime_error(fmt::format("Invalid target segment length: {}", ::param.target_segment_length));
        }
    };
    ::param.target_segment_length = *target_segment_length;
}

std::shared_ptr<cyHairFile> cmd::exec::resample(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    const bool has_segments = hairfile_in->GetSegmentsArray() != nullptr;
    const bool has_thickness = hairfile_in->GetThicknessArray() != nullptr;
    const bool has_transparency = hairfile_in->GetTransparencyArray() != nullptr;
    const bool has_color = hairfile_in->GetColorsArray() != nullptr;

    const unsigned int hair_count = header_in.hair_count;

    std::vector<std::vector<float>> points_per_strand(hair_count);
    std::vector<std::vector<float>> thickness_per_strand(hair_count);
    std::vector<std::vector<float>> transparency_per_strand(hair_count);
    std::vector<std::vector<float>> color_per_strand(hair_count);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        if (i > 0 && i % 100 == 0)
            spdlog::debug("Processing hair {}/{}", i, hair_count);

        const unsigned int num_segments = has_segments ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments;

        std::vector<unsigned int> j_range(num_segments);
        std::iota(j_range.begin(), j_range.end(), 0);

        // Determine number of subsegments for each segment
        std::vector<float> segment_length(num_segments);
        std::vector<unsigned int> num_subsegments_per_segment(num_segments);

        for (unsigned int j : j_range) {
            const Vector3f p0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j));
            const Vector3f p1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j + 1));

            segment_length[j] = (p1 - p0).norm();

            num_subsegments_per_segment[j] = (unsigned int)std::ceil(segment_length[j] / ::param.target_segment_length);
        }

        const unsigned int num_subsegments_total = std::accumulate(num_subsegments_per_segment.begin(), num_subsegments_per_segment.end(), 0);

        // Allocate memory
        points_per_strand[i].reserve(3 * (num_subsegments_total + 1));
        if (has_thickness) thickness_per_strand[i].reserve(num_subsegments_total + 1);
        if (has_transparency) transparency_per_strand[i].reserve(num_subsegments_total + 1);
        if (has_color) color_per_strand[i].reserve(3 * (num_subsegments_total + 1));

        for (unsigned int j : j_range) {
            const Vector3f point0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j));
            const Vector3f point1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j + 1));

            const std::optional<float> thickness0 = has_thickness ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + j]) : std::nullopt;
            const std::optional<float> thickness1 = has_thickness ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + j + 1]) : std::nullopt;
            const std::optional<float> transparency0 = has_transparency ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + j]) : std::nullopt;
            const std::optional<float> transparency1 = has_transparency ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + j + 1]) : std::nullopt;
            const std::optional<Vector3f> color0 = has_color ? std::optional<Vector3f>(Map<Vector3f>(hairfile_in->GetColorsArray() + 3*(offset + j))) : std::nullopt;
            const std::optional<Vector3f> color1 = has_color ? std::optional<Vector3f>(Map<Vector3f>(hairfile_in->GetColorsArray() + 3*(offset + j + 1))) : std::nullopt;

            // Add first point
            if (j == 0) {
                points_per_strand[i].insert(points_per_strand[i].end(), point0.data(), point0.data() + 3);
                if (has_thickness) thickness_per_strand[i].push_back(*thickness0);
                if (has_transparency) transparency_per_strand[i].push_back(*transparency0);
                if (has_color) color_per_strand[i].insert(color_per_strand[i].end(), color0->data(), color0->data() + 3);
            }

            const Vector3f delta_point = (point1 - point0) / num_subsegments_per_segment[j];

            const std::optional<float> delta_thickness = has_thickness ? std::optional<float>((*thickness1 - *thickness0) / num_subsegments_per_segment[j]) : std::nullopt;
            const std::optional<float> delta_transparency = has_transparency ? std::optional<float>((*transparency1 - *transparency0) / num_subsegments_per_segment[j]) : std::nullopt;
            const std::optional<Vector3f> delta_color = has_color ? std::optional<Vector3f>((*color1 - *color0) / num_subsegments_per_segment[j]) : std::nullopt;

            Vector3f curr_point = point0;
            std::optional<float> curr_thickness = thickness0;
            std::optional<float> curr_transparency = transparency0;
            std::optional<Vector3f> curr_color = color0;

            for (unsigned int k = 0; k < num_subsegments_per_segment[j]; ++k) {
                curr_point += delta_point;
                if (has_thickness) *curr_thickness += *delta_thickness;
                if (has_transparency) *curr_transparency += *delta_transparency;
                if (has_color) *curr_color += *delta_color;

                points_per_strand[i].insert(points_per_strand[i].end(), curr_point.data(), curr_point.data() + 3);
                if (has_thickness) thickness_per_strand[i].push_back(*curr_thickness);
                if (has_transparency) transparency_per_strand[i].push_back(*curr_transparency);
                if (has_color) color_per_strand[i].insert(color_per_strand[i].end(), curr_color->data(), curr_color->data() + 3);
            }
        }

        offset += num_segments + 1;
    }

    const unsigned int num_points_total = std::accumulate(points_per_strand.begin(), points_per_strand.end(), 0,
        [](unsigned int sum, const std::vector<float>& v){ return sum + v.size() / 3; }
    );

    // Create output hair file
    std::shared_ptr<cyHairFile> hairfile_out = std::make_shared<cyHairFile>();

    // Copy header via memcpy
    std::memcpy((void*)&hairfile_out->GetHeader(), &header_in, sizeof(cyHairFile::Header));

    // Set hair_count & point_count, allocate arrays
    hairfile_out->SetHairCount(hair_count);
    hairfile_out->SetPointCount(num_points_total);
    hairfile_out->SetArrays(header_in.arrays | _CY_HAIR_FILE_SEGMENTS_BIT);

    // Copy data to arrays of hairfile_out
    offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        const size_t num_points_per_strand = points_per_strand[i].size() / 3;

        hairfile_out->GetSegmentsArray()[i] = num_points_per_strand - 1;

        std::memcpy(hairfile_out->GetPointsArray() + 3*offset, points_per_strand[i].data(), 3*num_points_per_strand*sizeof(float));

        if (has_thickness) std::memcpy(hairfile_out->GetThicknessArray() + offset, thickness_per_strand[i].data(), num_points_per_strand*sizeof(float));
        if (has_transparency) std::memcpy(hairfile_out->GetTransparencyArray() + offset, transparency_per_strand[i].data(), num_points_per_strand*sizeof(float));
        if (has_color) std::memcpy(hairfile_out->GetColorsArray() + 3*offset, color_per_strand[i].data(), 3*num_points_per_strand*sizeof(float));

        offset += num_points_per_strand;
    }

    return hairfile_out;
}
