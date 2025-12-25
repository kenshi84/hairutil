#include "cmd.h"

#include <igl/min_quad_with_fixed.h>
#include <igl/speye.h>

using namespace Eigen;

namespace {
struct {
    float& lambda = cmd::param::f("smooth", "lambda");
} param;
}

void cmd::parse::smooth(args::Subparser &parser) {
    args::ValueFlag<float> lambda(parser, "R", "Smoothness weight [1.0]", {"lambda"}, 1.0f);
    parser.Parse();
    globals::cmd_exec = cmd::exec::smooth;
    globals::output_file_wo_ext = [](){ return fmt::format("{}_smoothed_λ_{}", globals::input_file_wo_ext, ::param.lambda); };
    globals::check_error = []() {
        if (::param.lambda <= 0) {
            throw std::runtime_error("Smoothness weight must be positive");
        }
    };
    ::param.lambda = *lambda;
}

std::shared_ptr<cyHairFile> cmd::exec::smooth(std::shared_ptr<cyHairFile> hairfile) {
    const int hair_count = hairfile->GetHeader().hair_count;

    int offset = 0;
    for (int i = 0; i < hair_count; ++i) {
        if (i > 0 && i % 100 == 0)
            spdlog::debug("Processing hair {}/{}", i, hair_count);

        const int nsegs = hairfile->GetSegmentsArray() ? hairfile->GetSegmentsArray()[i] : hairfile->GetHeader().d_segments;
        const int n = nsegs + 1;

        if (nsegs < 2)
            continue;

        // Copy point data to Eigen matrix
        MatrixX3d f = Map<Matrix3Xf>(hairfile->GetPointsArray() + 3*offset, 3, nsegs+1).cast<double>().transpose();

        /*
        Energy to be minimized for coordinate function f:
            E_data(f) = 1/2 Σ_i (f_i - f_0_i)^2
                      = 1/2 |f - f_0|^2
                      = 1/2 f^T I f + f^T (-f_0) + const.
            E_smooth(f) = 1/2 Σ_i (f_i+1 - f_i)^2
                        = 1/2 (D f)^2
                        = 1/2 f^T (D^T D) f
            E(f) = E_data(f) + λ E_smooth(f)
                 = 1/2 f^T (I + λ D^T D) f + f^T (-f_0) + const.
        */

        SparseMatrix<double> I;
        igl::speye<double>(n, I);

        SparseMatrix<double> D(n-1, n);
        std::vector<Triplet<double>> D_triplets(2*(n-1));
        for (int j = 0; j < n-1; ++j) {
            D_triplets[2*j+0] = {j, j, -1};
            D_triplets[2*j+1] = {j, j+1, 1};
        }
        D.setFromTriplets(D_triplets.begin(), D_triplets.end());

        const SparseMatrix<double> Q = I + ::param.lambda*(D.transpose()*D);
        const VectorXi b = (VectorXi(2) << 0, n - 1).finished();
        const MatrixX3d bc = (MatrixX3d(2, 3) << f.row(0), f.row(n - 1)).finished();
        const SparseMatrix<double> Aeq;
        const VectorXd Beq;
        igl::min_quad_with_fixed_data<double> mqwf;
        igl::min_quad_with_fixed_precompute(Q, b, Aeq, true, mqwf);
        igl::min_quad_with_fixed_solve(mqwf, -f, bc, Beq, f);

        // Copy resulting point data back to hairfile
        Matrix3Xf f_T = f.transpose().cast<float>();
        std::memcpy(hairfile->GetPointsArray() + 3*offset, f_T.data(), 3*(nsegs+1)*sizeof(float));

        offset += nsegs + 1;
    }
    return hairfile;
}
