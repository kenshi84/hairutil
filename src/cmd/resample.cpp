#include "cmd.h"

using namespace Eigen;

void cmd::parse::resample(args::Subparser &parser) {
    parser.Parse();
    globals::cmd_exec = cmd::exec::resample;
    globals::output_file = [](){ return globals::input_file_wo_ext + "_resampled." + globals::output_ext; };
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

        // Compute segment length
        std::vector<float> segment_length(num_segments);
        for (unsigned int j = 0; j < num_segments; ++j) {
            const Vector3f& p0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j));
            const Vector3f& p1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
            segment_length[j] = (p1 - p0).norm();
        }

        // Get the minimum segment length
        const float segment_length_min = *std::min_element(segment_length.begin(), segment_length.end());

        // Subdivide each segment
        std::vector<std::vector<float>> points_per_segment(num_segments);
        std::vector<std::vector<float>> thickness_per_segment(num_segments);
        std::vector<std::vector<float>> transparency_per_segment(num_segments);
        std::vector<std::vector<float>> color_per_segment(num_segments);

        for (unsigned int j = 0; j < num_segments; ++j) {
            const unsigned int num_subsegments = std::floor(segment_length[j] / segment_length_min);

            // Point
            points_per_segment[j].resize(3*num_subsegments);
            const Vector3f& p0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j));
            const Vector3f& p1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
            Vector3f p = p0;
            Vector3f dp = (p1 - p0) / num_subsegments;
            for (unsigned int k = 0; k < num_subsegments; ++k, p += dp) {
                std::memcpy(points_per_segment[j].data() + 3*k, p.data(), 3*sizeof(float));
            }

            // Thickness
            if (has_thickness) {
                thickness_per_segment[j].resize(num_subsegments);
                const float& t0 = hairfile_in->GetThicknessArray()[offset + j];
                const float& t1 = hairfile_in->GetThicknessArray()[offset + j + 1];
                const float dt = (t1 - t0) / num_subsegments;
                for (unsigned int k = 0; k < num_subsegments; ++k) {
                    thickness_per_segment[j][k] = t0 + k * dt;
                }
            }

            // Transparency
            if (has_transparency) {
                transparency_per_segment[j].resize(num_subsegments);
                const float& t0 = hairfile_in->GetTransparencyArray()[offset + j];
                const float& t1 = hairfile_in->GetTransparencyArray()[offset + j + 1];
                const float dt = (t1 - t0) / num_subsegments;
                for (unsigned int k = 0; k < num_subsegments; ++k) {
                    transparency_per_segment[j][k] = t0 + k * dt;
                }
            }

            // Color
            if (has_color) {
                color_per_segment[j].resize(3*num_subsegments);
                const Vector3f& c0 = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + j));
                const Vector3f& c1 = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + j + 1));
                Vector3f c = c0;
                Vector3f dc = (c1 - c0) / num_subsegments;
                for (unsigned int k = 0; k < num_subsegments; ++k, c += dc) {
                    std::memcpy(color_per_segment[j].data() + 3*k, c.data(), 3*sizeof(float));
                }
            }
        }

        // Determine the total number of subsegments
        const unsigned int num_subsegments_total = std::accumulate(points_per_segment.begin(), points_per_segment.end(), 0, [](unsigned int sum, const std::vector<float>& v){ return sum + v.size() / 3; });

        // Merge all the subdivided segments
        points_per_strand[i].reserve(3*(num_subsegments_total + 1));
        for (unsigned int j = 0; j < num_segments; ++j) {
            points_per_strand[i].insert(points_per_strand[i].end(), points_per_segment[j].begin(), points_per_segment[j].end());
        }
        if (has_thickness) {
            thickness_per_strand[i].reserve(num_subsegments_total + 1);
            for (unsigned int j = 0; j < num_segments; ++j) {
                thickness_per_strand[i].insert(thickness_per_strand[i].end(), thickness_per_segment[j].begin(), thickness_per_segment[j].end());
            }
        }
        if (has_transparency) {
            transparency_per_strand[i].reserve(num_subsegments_total + 1);
            for (unsigned int j = 0; j < num_segments; ++j) {
                transparency_per_strand[i].insert(transparency_per_strand[i].end(), transparency_per_segment[j].begin(), transparency_per_segment[j].end());
            }
        }
        if (has_color) {
            color_per_strand[i].reserve(3*(num_subsegments_total + 1));
            for (unsigned int j = 0; j < num_segments; ++j) {
                color_per_strand[i].insert(color_per_strand[i].end(), color_per_segment[j].begin(), color_per_segment[j].end());
            }
        }

        // Append the last point
        const Vector3f& p_last = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + num_segments));
        points_per_strand[i].insert(points_per_strand[i].end(), p_last.data(), p_last.data() + 3);
        if (has_thickness) {
            const float& t_last = hairfile_in->GetThicknessArray()[offset + num_segments];
            thickness_per_strand[i].push_back(t_last);
        }
        if (has_transparency) {
            const float& t_last = hairfile_in->GetTransparencyArray()[offset + num_segments];
            transparency_per_strand[i].push_back(t_last);
        }
        if (has_color) {
            const Vector3f& c_last = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + num_segments));
            color_per_strand[i].insert(color_per_strand[i].end(), c_last.data(), c_last.data() + 3);
        }

        offset += num_segments + 1;
    }

    const unsigned int num_points_total = std::accumulate(points_per_strand.begin(), points_per_strand.end(), 0, [](unsigned int sum, const std::vector<float>& v){ return sum + v.size() / 3; });

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
        if (has_thickness) {
            std::memcpy(hairfile_out->GetThicknessArray() + offset, thickness_per_strand[i].data(), num_points_per_strand*sizeof(float));
        }
        if (has_transparency) {
            std::memcpy(hairfile_out->GetTransparencyArray() + offset, transparency_per_strand[i].data(), num_points_per_strand*sizeof(float));
        }
        if (has_color) {
            std::memcpy(hairfile_out->GetColorsArray() + 3*offset, color_per_strand[i].data(), 3*num_points_per_strand*sizeof(float));
        }

        offset += num_points_per_strand;
    }

    return hairfile_out;
}
