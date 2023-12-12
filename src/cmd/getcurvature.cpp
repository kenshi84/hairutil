#include "cmd.h"

#include <highfive/H5Easy.hpp>

using namespace Eigen;

namespace {
struct {
    float angle_threshold;
} param;
}

void cmd::parse::getcurvature(args::Subparser &parser)
{
    args::ValueFlag<float> angle_threshold(parser, "R", "Angle threshold for determining straightness (in degrees) [1.0e-6]", {"angle-threshold"}, 1.0e-6);
    parser.Parse();
    globals::cmd_exec = &cmd::exec::getcurvature;
    globals::output_file = []() { return globals::input_file_wo_ext + "_cvtr.hdf5"; };
    ::param = {};
    ::param.angle_threshold = *angle_threshold;
}

std::shared_ptr<cyHairFile> cmd::exec::getcurvature(std::shared_ptr<cyHairFile> hairfile) {
    H5Easy::File file(globals::output_file(), H5Easy::File::Overwrite);

    const int num_strands = hairfile->GetHeader().hair_count;
    H5Easy::dump(file, "/num_strands", num_strands);

    std::vector<int> nsegs(num_strands);
    for (int i = 0; i < num_strands; ++i) {
        nsegs[i] = hairfile->GetSegmentsArray() ? hairfile->GetSegmentsArray()[i] : hairfile->GetHeader().d_segments;
    }
    H5Easy::dump(file, "/nsegs", nsegs);

    int offset = 0;
    for (int i = 0; i < num_strands; ++i) {
        // Copy point data to Eigen array
        MatrixX3f point(nsegs[i]+1, 3);
        for (int j = 0; j < nsegs[i]+1; ++j)
            point.row(j) = Map<const RowVector3f>(hairfile->GetPointsArray() + 3*(offset+j));

        // Get unit tangent vector
        const MatrixX3f edge = point.block(1, 0, nsegs[i], 3) - point.block(0, 0, nsegs[i], 3);
        const VectorXf edge_length = edge.rowwise().norm();
        const MatrixX3f tangent = edge.array().colwise() / edge_length.array();
        const VectorXf vertex_length = 0.5 * (edge_length.head(nsegs[i]-1) + edge_length.tail(nsegs[i]-1));

        // Get cross-product of consecutive tangent vectors
        MatrixX3f tangent_cross(nsegs[i]-1, 3);
        for (int j = 0; j < nsegs[i]-1; ++j)
            tangent_cross.row(j) = tangent.row(j).cross(tangent.row(j+1));
        const VectorXf tangent_cross_norm = tangent_cross.rowwise().norm();
        const VectorXf turning_angle = tangent_cross_norm.array().asin();

        const VectorXi is_straight = (turning_angle.array() < ::param.angle_threshold * std::numbers::pi / 180.0).cast<int>();
        if (is_straight.sum() == nsegs[i]-1) {
            throw std::runtime_error(fmt::format("Strand {} is completely straight", i));
        }

        MatrixX3f binormal = tangent_cross.array().colwise() / tangent_cross_norm.array();

        // Find indices where is_straight(j) != is_straight(j+1)
        std::vector<int> transition;
        for (int j = 0; j < nsegs[i]-2; ++j) {
            if (is_straight(j) != is_straight(j+1))
                transition.push_back(j);
        }
        // Process each straight interval
        for (auto t = transition.begin(); transition.size() >= 2 && t+1 != transition.end(); ++t) {
            int t1 = *t;
            int t2 = *(t+1);
            if (!is_straight(t1)) {
                // The interval [t1+1, t2] is straight
                const Vector3f binormal_0 = binormal.row(t1);
                Vector3f binormal_1 = binormal.row(t2+1);

                // Make sure binormal_1 is in the same direction as binormal_0
                if (binormal_0.dot(binormal_1) < 0.0) {
                    binormal_1 = -binormal_1;
                    // Flip binormal for the rest of the strand
                    for (int j = t2+1; j < nsegs[i]-1; ++j)
                        binormal.row(j) = -binormal.row(j);
                }

                // Rotate binormals in the interval by linearly interpolated angle
                const float angle = std::acos(std::clamp(binormal_0.dot(binormal_1), -1.0f, 1.0f));
                const Vector3f axis = binormal_0.cross(binormal_1).normalized();
                float total_length = 0;
                for (int j = t1+1; j <= t2+1; ++j) {
                    total_length += edge_length(j);
                }
                float curr_length = 0;
                for (int j = t1+1; j <= t2; ++j) {
                    curr_length += edge_length(j);
                    const float theta = angle * curr_length / total_length;
                    binormal.row(j) = AngleAxisf(theta, axis) * binormal_0;
                }
            }
        }
        // If the front and back ends are straight, simply copy the adjacent binormal
        if (!transition.empty() && is_straight(transition.front())) {
            for (int j = 0; j <= transition.front(); ++j)
                binormal.row(j) = binormal.row(transition.front()+1);
        }
        if (!transition.empty() && !is_straight(transition.back())) {
            for (int j = transition.back()+1; j < nsegs[i]-1; ++j)
                binormal.row(j) = binormal.row(transition.back());
        }

        // Compute curvature
        std::vector<float> kappa(nsegs[i]-1);
        for (int j = 0; j < nsegs[i]-1; ++j) {
            if (is_straight(j)) {
                kappa[j] = 0.0;
            } else {
                kappa[j] = turning_angle(j) / vertex_length(j);
                if (tangent_cross.row(j).dot(binormal.row(j)) < 0)
                    kappa[j] = -kappa[j];
            }
        }

        // Compute torsion
        std::vector<float> tau(nsegs[i]-2);
        for (int j = 0; j < nsegs[i]-2; ++j) {
            const Vector3f binormal_cross = binormal.row(j).cross(binormal.row(j+1));
            const float angle = std::asin(std::clamp(binormal_cross.norm(), -1.0f, 1.0f));
            tau[j] = angle / edge_length(j+1);
            if (binormal_cross.dot(tangent.row(j+1)) < 0)
                tau[j] = -tau[j];
        }

        H5Easy::dump(file, fmt::format("/{}/binormal", i), binormal);
        H5Easy::dump(file, fmt::format("/{}/kappa", i), kappa);
        H5Easy::dump(file, fmt::format("/{}/tau", i), tau);
        H5Easy::dump(file, fmt::format("/{}/length", i), edge_length);

        offset += nsegs[i] + 1;
    }

    return {};
}
