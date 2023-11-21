#include "cmd.h"
#include "util.h"

#include "kdtree.h"

using namespace Eigen;

namespace {
unsigned int target_count;
float scale_factor;
std::set<int> indices;
}

void cmd::parse::subsample(args::Subparser &parser) {
    args::ValueFlag<unsigned int> target_count(parser, "N", "(*)Target number of hair strands", {"target-count"}, 0);
    args::ValueFlag<float> scale_factor(parser, "R", "Factor for scaling down the Poisson disk radius [0.9]", {"scale-factor"}, 0.9);
    args::ValueFlag<std::string> indices(parser, "N,...", "Comma-separated list of strand indices to extract", {"indices"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::subsample;
    globals::output_file = []() -> std::string {
        if (::indices.empty()) {
            return fmt::format("{}_{}.{}", globals::input_file_wo_ext, ::target_count, globals::output_ext);
        } else {
            std::string indices_str = util::join_vector_to_string(::indices, '_');
            if (indices_str.size() > 100)
                indices_str.resize(100);
            return fmt::format("{}_indices_{}.{}", globals::input_file_wo_ext, indices_str, globals::output_ext);
        }
    };
    globals::check_error = [](){
        if (::scale_factor >= 1.0) {
            throw std::runtime_error("--scale-factor must be less than 1.0");
        }
    };
    if (!target_count && !indices || target_count && indices) {
        throw std::runtime_error("Either --target-count or --indices (not both) must be specified");
    }
    ::target_count = *target_count;
    ::scale_factor = *scale_factor;

    if (indices) {
        const std::vector<int> indices_vec = util::parse_comma_separated_values<int>(*indices);
        ::indices.insert(indices_vec.begin(), indices_vec.end());
    }
}

std::shared_ptr<cyHairFile> cmd::exec::subsample(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    if (header_in.hair_count < ::target_count) {
        throw std::runtime_error("Target number of hair strands must be less than the number of hair strands in the input file");
    }

    // Flag for whether a strand is selected
    std::vector<unsigned char> selected(header_in.hair_count, 0);

    // Forward declare because of goto
    KdTree3d kdtree;
    AlignedBox3d bbox;
    unsigned int in_point_offset;
    double r;
    std::uniform_int_distribution<int> uniform_dist(0, header_in.hair_count - 1);
    unsigned int num_selected;

    if (!::indices.empty()) {
        for (unsigned int i = 0; i < header_in.hair_count; ++i) {
            if (::indices.count(i)) {
                selected[i] = 1;
            }
        }
        return util::get_subset(hairfile_in, selected);
    }

    // Collect all the root points and build a kdtree
    kdtree.points.resize(header_in.hair_count, 3);
    in_point_offset = 0;
    for (int i = 0; i < header_in.hair_count; ++i) {
        Vector3d point = Map<Vector3f>(hairfile_in->GetPointsArray() + 3 * in_point_offset).cast<double>();
        kdtree.points.row(i) = point.transpose();
        in_point_offset += (hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments) + 1;
    }
    kdtree.build();

    // Get the bounding box of all the root points
    for (int i = 0; i < header_in.hair_count; ++i)
        bbox.extend(kdtree.points.row(i).transpose());

    // Init the Poisson disk radius to half the diagonal of the bounding box
    r = bbox.diagonal().norm() * 0.5;

    // Loop while the number of selected strands is below target
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

    return util::get_subset(hairfile_in, selected);
}
