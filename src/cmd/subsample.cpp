#include "cmd.h"

#include "kdtree.h"

using namespace Eigen;

namespace {
unsigned int target_count;
float scale_factor;
}

void cmd::parse::subsample(args::Subparser &parser) {
    args::ValueFlag<unsigned int> target_count(parser, "N", "(*)Target number of hair strands", {"target-count"}, args::Options::Required);
    args::ValueFlag<float> scale_factor(parser, "R", "Factor for scaling down the Poisson disk radius [0.9]", {"scale-factor"}, 0.9);
    parser.Parse();
    globals::cmd_exec = cmd::exec::subsample;
    globals::output_file = [](){ return globals::input_file_wo_ext + "_" + std::to_string(::target_count) + "." + globals::output_ext; };
    globals::check_error = [](){
        if (::scale_factor >= 1.0) {
            throw std::runtime_error("--scale-factor must be less than 1.0");
        }
    };
    ::target_count = *target_count;
    ::scale_factor = *scale_factor;
}

std::shared_ptr<cyHairFile> cmd::exec::subsample(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    if (header_in.hair_count < ::target_count) {
        throw std::runtime_error("Target number of hair strands must be less than the number of hair strands in the input file");
    }

    // Flag for whether a strand is selected
    std::vector<unsigned char> selected(header_in.hair_count, 0);

    // Collect all the root points and build a kdtree
    KdTree3d kdtree;
    kdtree.points.resize(header_in.hair_count, 3);
    unsigned int in_point_offset = 0;
    for (int i = 0; i < header_in.hair_count; ++i) {
        Vector3d point = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * in_point_offset).cast<double>();
        kdtree.points.row(i) = point.transpose();
        in_point_offset += (hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments) + 1;
    }
    kdtree.build();

    // Get the bounding box of all the root points
    AlignedBox3d bbox;
    for (int i = 0; i < header_in.hair_count; ++i)
        bbox.extend(kdtree.points.row(i).transpose());

    // Init the Poisson disk radius to half the diagonal of the bounding box
    double r = bbox.diagonal().norm() * 0.5;

    std::uniform_int_distribution<int> uniform_dist(0, header_in.hair_count - 1);

    // Loop while the number of selected strands is below target
    unsigned int num_selected;
    for ( ; (num_selected = std::accumulate(selected.begin(), selected.end(), 0)) < ::target_count; )
    {
        if (num_selected && num_selected % 100 == 0)
            spdlog::info("Selected {} strands", num_selected);

        // Flag root points that are covered by the current Poisson disk of selected strands
        std::vector<unsigned char> covered(header_in.hair_count, 0);
        for (int i = 0; i < header_in.hair_count; ++i)
        {
            // For each selected strand
            if (selected[i])
            {
                // Perform radius search on kdtree
                KdTreeSearchResult res;
                kdtree.radiusSearch(kdtree.points.row(i), r, res);

                // Flag the found points as covered
                for (size_t j = 0; j < res.indices.size(); ++j)
                    covered[res.indices[j]] = 1;
            }
        }

        // If all points are covered, reduce the Poisson disk radius
        if (std::accumulate(covered.begin(), covered.end(), 0) == header_in.hair_count)
        {
            r *= ::scale_factor;
        }
        else
        {
            // Randomly select one uncovered root point
            int i = uniform_dist(globals::rng);                              // Start from a random point
            while (covered[i]) { i = (i + 1) % header_in.hair_count; }      // Find the first uncovered point
            selected[i] = 1;                                                // Flag it as selected
        }
    }

    // Count the number of selected strands and their points
    unsigned int out_hair_count = num_selected;
    unsigned int out_point_count = 0;
    for (int i = 0; i < header_in.hair_count; ++i)
    {
        if (selected[i])
            out_point_count += (hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments) + 1;
    }

    // Create output hair file
    std::shared_ptr<cyHairFile> hairfile_out = std::make_shared<cyHairFile>();

    // Copy header via memcpy
    memcpy((void*)&hairfile_out->GetHeader(), &header_in, sizeof(cyHairFile::Header));

    // Set hair_count & point_count, allocate arrays
    hairfile_out->SetHairCount(out_hair_count);
    hairfile_out->SetPointCount(out_point_count);
    hairfile_out->SetArrays(header_in.arrays);

    // Copy array data
    in_point_offset = 0;
    unsigned int out_hair_idx = 0;
    unsigned int out_point_offset = 0;
    for (unsigned int in_hair_idx = 0; in_hair_idx < header_in.hair_count; ++in_hair_idx)
    {
        const unsigned int nsegs = hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[in_hair_idx] : header_in.d_segments;

        if (selected[in_hair_idx])
        {
            // Copy segment info if available
            if (header_in.arrays & _CY_HAIR_FILE_SEGMENTS_BIT)
                hairfile_out->GetSegmentsArray()[out_hair_idx++] = nsegs;

            // Copy per-point info if available
            for (int j = 0; j < nsegs + 1; ++j)
            {
                if (header_in.arrays & _CY_HAIR_FILE_POINTS_BIT)
                {
                    for (int k = 0; k < 3; ++k)
                        hairfile_out->GetPointsArray()[3*(out_point_offset + j) + k] = hairfile_in->GetPointsArray()[3*(in_point_offset + j) + k];
                }

                if (header_in.arrays & _CY_HAIR_FILE_THICKNESS_BIT)
                    hairfile_out->GetThicknessArray()[out_point_offset + j] = hairfile_in->GetThicknessArray()[in_point_offset + j];

                if (header_in.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT)
                    hairfile_out->GetTransparencyArray()[out_point_offset + j] = hairfile_in->GetTransparencyArray()[in_point_offset + j];

                if (header_in.arrays & _CY_HAIR_FILE_COLORS_BIT)
                {
                    for (int k = 0; k < 3; ++k)
                        hairfile_out->GetColorsArray()[3*(out_point_offset + j) + k] = hairfile_in->GetColorsArray()[3*(in_point_offset + j) + k];
                }
            }
            out_point_offset += nsegs + 1;
        }
        in_point_offset += nsegs + 1;
    }
    if (header_in.arrays & _CY_HAIR_FILE_SEGMENTS_BIT)
        assert(out_hair_idx == out_hair_count);

    return hairfile_out;
}
