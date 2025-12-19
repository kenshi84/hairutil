#include "cmd.h"

#include <igl/read_triangle_mesh.h>
#include <igl/fast_winding_number.h>
#include <igl/decimate.h>

using namespace Eigen;

namespace {
struct {
std::string& mesh_path = cmd::param::s("findpenet", "mesh_path");
float& decimate_ratio = cmd::param::f("findpenet", "decimate_ratio");
float& threshold_ratio = cmd::param::f("findpenet", "threshold_ratio");
bool& no_export = cmd::param::b("findpenet", "no_export");
bool& no_print = cmd::param::b("findpenet", "no_print");
} param;
}

void cmd::parse::findpenet(args::Subparser &parser) {
    args::ValueFlag<std::string> mesh_path(parser, "PATH", "(REQUIRED) Path to triangle mesh", {'m', "mesh-path"}, args::Options::Required);
    args::ValueFlag<float> decimate_ratio(parser, "RATIO", "Ratio for decimating triangle mesh [0.25]", {'d', "decimate-ratio"}, 0.25f);
    args::ValueFlag<float> threshold_ratio(parser, "RATIO", "Threshold ratio [0.3]; detect strand as penetrating if #in-points is more than this value times #total-points", {'t', "threshold-ratio"}, 0.3f);
    args::Flag no_export(parser, "no-export", "Do not export result to txt", {"no-export"});
    args::Flag no_print(parser, "no-print", "Do not print result to stdout", {"no-print"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::findpenet;
    globals::check_error = [](){
        if (::param.decimate_ratio <= 0.0f || ::param.decimate_ratio > 1.0f) {
            throw std::runtime_error("--decimate-ratio must be in (0.0, 1.0]");
        }
        if (::param.threshold_ratio < 0.0f || ::param.threshold_ratio > 1.0f) {
            throw std::runtime_error("--threshold-ratio must be in [0.0, 1.0]");
        }
    };
    ::param.mesh_path = *mesh_path;
    ::param.decimate_ratio = *decimate_ratio;
    ::param.threshold_ratio = *threshold_ratio;
    ::param.no_export = no_export;
    ::param.no_print = no_print;
    if (!::param.no_export) {
        globals::output_file = [](){ return fmt::format("{}_penet.txt", globals::input_file_wo_ext); };
    }
}

std::shared_ptr<cyHairFile> cmd::exec::findpenet(std::shared_ptr<cyHairFile> hairfile) {
    // Read triangle mesh
    spdlog::info("Reading triangle mesh from {}", ::param.mesh_path);
    MatrixXd OV;
    MatrixXi OF;
    if (!igl::read_triangle_mesh(::param.mesh_path, OV, OF)) {
        throw std::runtime_error("Failed to read triangle mesh");
    }

    // Decimate triangle mesh
    MatrixXd V;
    MatrixXi F;
    if (::param.decimate_ratio < 1.0f) {
        spdlog::info("Decimating triangle mesh with ratio {} ({} faces)", ::param.decimate_ratio, (int)(::param.decimate_ratio * OF.rows()));
        VectorXi J, I;
        igl::decimate(OV, OF, ::param.decimate_ratio * OF.rows(), false, V, F, J, I);
    } else {
        V = OV;
        F = OF;
    }

    // Convert point array in hairfile to Eigen::MatrixXd
    const auto& header = hairfile->GetHeader();
    MatrixXd P(header.point_count, 3);
    for (unsigned int i = 0; i < header.point_count; i++) {
        P.row(i) = Map<RowVector3f>(hairfile->GetPointsArray() + 3 * i).cast<double>();
    }

    // Compute winding number
    spdlog::info("Computing winding number");
    VectorXd W;
    igl::FastWindingNumberBVH fwn_bvh;
    igl::fast_winding_number(V,F,2,fwn_bvh);
    igl::fast_winding_number(fwn_bvh,2,P,W);

    // Determine which strands are penetrating
    spdlog::info("Determining which strands are penetrating");
    std::set<unsigned int> penetrating;
    unsigned int offset = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned int nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile->GetSegmentsArray()[i] : header.d_segments;

        const double* begin = W.data() + offset;
        const double* end = begin + nsegs + 1;

        const unsigned int num_penetrating_points = std::count_if(begin, end, [](double x) { return x > 0.5; });

        if (num_penetrating_points > ::param.threshold_ratio * (nsegs+1)) {
            penetrating.insert(i);
        }

        offset += nsegs + 1;
    }

    if (!penetrating.empty()) {
        std::stringstream ss;
        std::copy(penetrating.begin(), penetrating.end(), std::ostream_iterator<unsigned int>(ss, ","));
        spdlog::warn("Found {} strands penetrating the mesh:\n{}", penetrating.size(), ::param.no_print ? std::string() : ss.str());
        if (!::param.no_export) {
            std::ofstream ofs(globals::output_file());
            ofs << ss.str();
        }
        spdlog::info("Exported penetrating strands to {}", globals::output_file());
    } else {
        spdlog::info("No penetrating strands found");
    }

    return {};
}
