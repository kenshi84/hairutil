#include "cmd.h"

using namespace Eigen;

namespace {
float ignore_segment_length_ratio;
unsigned int min_num_segments;
}

void cmd::parse::resample(args::Subparser &parser) {
    args::ValueFlag<float> ignore_segment_length_ratio(parser, "R", "Ignore segments shorter than this ratio times the maximum segment length [0.2]", {"ignore-segment-length-ratio"}, 0.2f);
    args::ValueFlag<unsigned int> min_num_segments(parser, "N", "Minimum number of segments per hair [20]", {"min-num-segments"}, 20);
    parser.Parse();
    globals::cmd_exec = cmd::exec::resample;
    globals::output_file = [](){ return globals::input_file_wo_ext + "_resampled." + globals::output_ext; };
    globals::check_error = [](){
        if (::ignore_segment_length_ratio < 0 || ::ignore_segment_length_ratio >= 1) {
            throw std::runtime_error(fmt::format("Invalid ignore segment length ratio: {}", ::ignore_segment_length_ratio));
        }
    };
    ::ignore_segment_length_ratio = *ignore_segment_length_ratio;
    ::min_num_segments = *min_num_segments;
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

        // Insert the first point
        const Vector3f& p_first = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * offset);
        points_per_strand[i].insert(points_per_strand[i].end(), p_first.data(), p_first.data() + 3);
        if (has_thickness) {
            const float& t_first = hairfile_in->GetThicknessArray()[offset];
            thickness_per_strand[i].push_back(t_first);
        }
        if (has_transparency) {
            const float& t_first = hairfile_in->GetTransparencyArray()[offset];
            transparency_per_strand[i].push_back(t_first);
        }
        if (has_color) {
            const Vector3f& c_first = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset));
            color_per_strand[i].insert(color_per_strand[i].end(), c_first.data(), c_first.data() + 3);
        }

        const unsigned int num_segments = has_segments ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments;

        // Compute segment length
        std::vector<float> segment_length(num_segments);
        std::vector<Vector3f> segment_delta(num_segments);
        for (unsigned int j = 0; j < num_segments; ++j) {
            const Vector3f& p0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j));
            const Vector3f& p1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
            segment_delta[j] = p1 - p0;
            segment_length[j] = segment_delta[j].norm();
        }

        // Get the maximum segment length
        const auto segment_length_max = std::max_element(segment_length.begin(), segment_length.end());
        const float ignore_segment_length_threshold = ignore_segment_length_ratio * (*segment_length_max);

        // Determine which segments to ignore
        std::vector<unsigned char> segment_ignored(num_segments);
        std::transform(segment_length.begin(), segment_length.end(), segment_ignored.begin(),
            [&](float l) { return l < ignore_segment_length_threshold; }
        );

        const unsigned int num_ignored_segments = std::accumulate(segment_ignored.begin(), segment_ignored.end(), 0,
            [](unsigned int sum, unsigned char b){ return sum + b; }
        );
        if (num_ignored_segments > 0) {
            spdlog::warn("Hair {} has {} ignored segments", i, num_ignored_segments);
            spdlog::debug("Segment lengths:");
            for (unsigned int j = 0; j < num_segments; ++j) {
                spdlog::debug("  {}: {}", j, segment_length[j]);
            }
            spdlog::debug("Max at {}: {}", std::distance(segment_length.begin(), segment_length_max), *segment_length_max);
            spdlog::debug("Length threshold: {}", ignore_segment_length_threshold);
            spdlog::debug("Ignored segments:");
            for (unsigned int j = 0; j < num_segments; ++j) {
                if (segment_ignored[j])
                    spdlog::debug("  {}: {}", j, segment_length[j]);
            }
        }

        // Get the minimum segment length among the non-ignored segments
        const float segment_length_min = [&](){
            float res = std::numeric_limits<float>::max();
            for (unsigned int j = 0; j < num_segments; ++j) {
                if (!segment_ignored[j])
                    res = std::min(res, segment_length[j]);
            }
            return res;
        }();

        // Determine the number of subdivision for each segment
        std::vector<unsigned int> num_subsegments(num_segments, 0);
        for (unsigned int j = 0; j < num_segments; ++j) {
            if (!segment_ignored[j])
                num_subsegments[j] = std::floor(segment_length[j] / segment_length_min);
        }

        // Additional subdivision needed to meet the min_num_segments condition
        const int num_additional_subdiv = std::max<int>(0, ::min_num_segments - std::accumulate(num_subsegments.begin(), num_subsegments.end(), 0));
        if (num_additional_subdiv > 0) {
            spdlog::warn("Hair {} needs {} additional subdivisions", i, num_additional_subdiv);
            for (int j = 0, k = 0; j < num_additional_subdiv; ++j) {
                while (true) {
                    k = (k + 1) % num_segments;
                    if (!segment_ignored[k]) {
                        ++num_subsegments[k];
                        break;
                    }
                }
            }
        }

        std::vector<std::vector<Vector3f>> deltas_per_segment(num_segments);
        std::vector<std::vector<float>> thickness_per_segment(num_segments);
        std::vector<std::vector<float>> transparency_per_segment(num_segments);
        std::vector<std::vector<float>> color_per_segment(num_segments);

        for (unsigned int j = 0; j < num_segments; ++j) {
            if (segment_ignored[j])
                continue;

            const unsigned int num_subsegments_j = num_subsegments[j];

            // Point
            deltas_per_segment[j].resize(num_subsegments_j, segment_delta[j] / num_subsegments_j);

            // Thickness
            if (has_thickness) {
                thickness_per_segment[j].resize(num_subsegments_j);
                const float& t0 = hairfile_in->GetThicknessArray()[offset + j];
                const float& t1 = hairfile_in->GetThicknessArray()[offset + j + 1];
                const float dt = (t1 - t0) / num_subsegments_j;
                float t = t0 + dt;
                for (unsigned int k = 0; k < num_subsegments_j; ++k, t += dt) {
                    thickness_per_segment[j][k] = t;
                }
            }

            // Transparency
            if (has_transparency) {
                transparency_per_segment[j].resize(num_subsegments_j);
                const float& t0 = hairfile_in->GetTransparencyArray()[offset + j];
                const float& t1 = hairfile_in->GetTransparencyArray()[offset + j + 1];
                const float dt = (t1 - t0) / num_subsegments_j;
                float t = t0 + dt;
                for (unsigned int k = 0; k < num_subsegments_j; ++k, t += dt) {
                    transparency_per_segment[j][k] = t;
                }
            }

            // Color
            if (has_color) {
                color_per_segment[j].resize(3*num_subsegments_j);
                const Vector3f& c0 = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + j));
                const Vector3f& c1 = Map<Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + j + 1));
                Vector3f dc = (c1 - c0) / num_subsegments_j;
                Vector3f c = c0 + dc;
                for (unsigned int k = 0; k < num_subsegments_j; ++k, c += dc) {
                    std::memcpy(color_per_segment[j].data() + 3*k, c.data(), 3*sizeof(float));
                }
            }
        }

        // Determine the total number of subsegments
        const unsigned int num_subsegments_total = std::accumulate(deltas_per_segment.begin(), deltas_per_segment.end(), 0, 
            [](unsigned int sum, const std::vector<Vector3f>& v){ return sum + v.size(); }
        );

        // Integrate all the subsegment deltas
        points_per_strand[i].reserve(3*(num_subsegments_total + 1));
        Vector3f p = Map<Vector3f>(points_per_strand[i].data());
        for (unsigned int j = 0; j < num_segments; ++j) {
            for (size_t k = 0; k < deltas_per_segment[j].size(); ++k) {
                p += deltas_per_segment[j][k];
                points_per_strand[i].insert(points_per_strand[i].end(), p.data(), p.data() + 3);
            }
        }
        // Merge all the subsegment arrays for thickness, transparency, and color
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
