#include "cmd.h"

#include <highfive/H5Easy.hpp>

using namespace Eigen;

#ifndef NDEBUG
#include <happly.h>
std::vector<float> tovector_f(VectorXf v) {
    std::vector<float> ret(v.size());
    for (int i = 0; i < v.size(); ++i)
        ret[i] = v[i];
    return ret;
}
std::vector<unsigned char> tovector_uchar(VectorXf v) {
    std::vector<unsigned char> ret(v.size());
    for (int i = 0; i < v.size(); ++i)
        ret[i] = (unsigned char)(255 * v[i]);
    return ret;
}
std::vector<int> tovector_int(VectorXi v) {
    std::vector<int> ret(v.size());
    for (int i = 0; i < v.size(); ++i)
        ret[i] = v[i];
    return ret;
}
#endif

namespace {
struct {
    float angle_threshold;
} param;
}

void cmd::parse::getcurvature(args::Subparser &parser)
{
    args::ValueFlag<float> angle_threshold(parser, "R", "Angle threshold for determining straightness (in degrees) [0.01]", {"angle-threshold"}, 0.01);
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
        if (i > 0 && i % 100 == 0)
            spdlog::debug("Processing hair {}/{}", i, num_strands);

        // Copy point data to Eigen array
        MatrixX3f point(nsegs[i]+1, 3);
        for (int j = 0; j < nsegs[i]+1; ++j)
            point.row(j) = Map<const RowVector3f>(hairfile->GetPointsArray() + 3*(offset+j));

        // Get unit tangent vector
        const MatrixX3f edge = point.block(1, 0, nsegs[i], 3) - point.block(0, 0, nsegs[i], 3);
        const VectorXf edge_length = edge.rowwise().norm();
        const MatrixX3f tangent = edge.array().colwise() / edge_length.array();
        const VectorXf vertex_length = 0.5 * (edge_length.head(nsegs[i]-1) + edge_length.tail(nsegs[i]-1));

        H5Easy::dump(file, fmt::format("/{}/edge_length", i), edge_length);

        // Get cross-product of consecutive tangent vectors
        MatrixX3f tangent_cross(nsegs[i]-1, 3);
        for (int j = 0; j < nsegs[i]-1; ++j)
            tangent_cross.row(j) = tangent.row(j).cross(tangent.row(j+1));
        const VectorXf tangent_cross_norm = tangent_cross.rowwise().norm();
        const VectorXf turning_angle = tangent_cross_norm.array().asin();

        const VectorXi is_straight = (turning_angle.array() < ::param.angle_threshold * globals::pi / 180.0).cast<int>();

        // If the strand is completely straight, simply set binormal to a random vector
        if (is_straight.sum() == nsegs[i]-1) {
            spdlog::warn("Strand {} is completely straight", i);
            RowVector3f binormal = RowVector3f::Random();
            binormal = (binormal - binormal.dot(tangent.row(0)) * tangent.row(0)).normalized();
            H5Easy::dump(file, fmt::format("/{}/binormal", i),  binormal.replicate(nsegs[i]-1, 1).eval());
            H5Easy::dump(file, fmt::format("/{}/kappa", i), std::vector<float>(nsegs[i]-1, 0.0));
            H5Easy::dump(file, fmt::format("/{}/tau", i), std::vector<float>(nsegs[i]-2, 0.0));
            continue;
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

        offset += nsegs[i] + 1;

#ifndef NDEBUG
        if (spdlog::get_level() <= spdlog::level::debug) {
            happly::PLYData ply;

            const int n = binormal.rows();

            VectorXf vertex_x(2*n);
            VectorXf vertex_y(2*n);
            VectorXf vertex_z(2*n);
            VectorXf vertex_red = ArrayXf::LinSpaced(n, 0.0, 1.0).replicate(2, 1);
            VectorXf vertex_green = vertex_red;
            VectorXf vertex_blue = vertex_red;
            vertex_x.head(n) = point.col(0).segment(1, n);
            vertex_y.head(n) = point.col(1).segment(1, n);
            vertex_z.head(n) = point.col(2).segment(1, n);
            vertex_x.tail(n) = vertex_x.head(n) + edge_length(0) * binormal.col(0);
            vertex_y.tail(n) = vertex_y.head(n) + edge_length(0) * binormal.col(1);
            vertex_z.tail(n) = vertex_z.head(n) + edge_length(0) * binormal.col(2);
            // MatrixX3f binormal_raw = tangent_cross.array().colwise() / tangent_cross_norm.array();
            // vertex_x.tail(n) = vertex_x.head(n) + edge_length(0) * binormal_raw.col(0);
            // vertex_y.tail(n) = vertex_y.head(n) + edge_length(0) * binormal_raw.col(1);
            // vertex_z.tail(n) = vertex_z.head(n) + edge_length(0) * binormal_raw.col(2);
            vertex_red.head(n) = VectorXf::Constant(n, 1.0);
            vertex_blue.tail(n) = VectorXf::Constant(n, 1.0);

            VectorXi edge_vertex1 = VectorXi::LinSpaced(n, 0, n-1);
            VectorXi edge_vertex2 = edge_vertex1.array() + n;

            ply.addElement("vertex", 2*n);
            ply.getElement("vertex").addProperty<float>("x", tovector_f(vertex_x));
            ply.getElement("vertex").addProperty<float>("y", tovector_f(vertex_y));
            ply.getElement("vertex").addProperty<float>("z", tovector_f(vertex_z));
            ply.getElement("vertex").addProperty<unsigned char>("red", tovector_uchar(vertex_red));
            ply.getElement("vertex").addProperty<unsigned char>("green", tovector_uchar(vertex_green));
            ply.getElement("vertex").addProperty<unsigned char>("blue", tovector_uchar(vertex_blue));

            ply.addElement("edge", n);
            ply.getElement("edge").addProperty<int>("vertex1", tovector_int(edge_vertex1));
            ply.getElement("edge").addProperty<int>("vertex2", tovector_int(edge_vertex2));

            ply.write(globals::input_file_wo_ext + "_cvtr_debug.ply", globals::ply_save_ascii ? happly::DataFormat::ASCII : happly::DataFormat::Binary);
            spdlog::debug("Wrote debug info to {}", globals::input_file_wo_ext + "_cvtr_debug.ply");
            break;
        }
#endif
    }
    spdlog::info("Written to {}", globals::output_file());

    return {};
}
