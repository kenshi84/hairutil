#include "cmd.h"

#include <igl/min_quad_with_fixed.h>
#include <igl/speye.h>

using namespace Eigen;

namespace {
struct {
    float& w0 = cmd::param::f("smooth", "w0");
    float& w1 = cmd::param::f("smooth", "w1");
    float& w2 = cmd::param::f("smooth", "w2");
    unsigned int& num_iter = cmd::param::ui("smooth", "num_iter");
} param;
}

void cmd::parse::smooth(args::Subparser &parser) {
    args::ValueFlag<float> w0(parser, "R", "Weight for the data term [1.0]", {"w0"}, 1.0f);
    args::ValueFlag<float> w1(parser, "R", "Weight for the first-order term [1.0]", {"w1"}, 1.0f);
    args::ValueFlag<float> w2(parser, "R", "Weight for the second-order term [1.0]", {"w2"}, 1.0f);
    args::ValueFlag<unsigned int> num_iter(parser, "N", "Number of smoothing iterations [1]", {"num-iter", 'n'}, 1);
    parser.Parse();
    globals::cmd_exec = cmd::exec::smooth;
    globals::output_file_wo_ext = [](){ return fmt::format("{}_smoothed_w0_{}_w1_{}_w2_{}_n_{}", globals::input_file_wo_ext, ::param.w0, ::param.w1, ::param.w2, ::param.num_iter); };
    globals::check_error = []() {
        if (::param.w0 < 0 || ::param.w1 < 0 || ::param.w2 < 0) {
            throw std::runtime_error("Weights must be non-negative");
        }
        if (::param.w0 == 0 && ::param.w1 == 0 && ::param.w2 == 0) {
            throw std::runtime_error("At least one weight must be positive");
        }
        if (::param.num_iter == 0) {
            throw std::runtime_error("Number of iterations must be positive");
        }
    };
    ::param.w0 = *w0;
    ::param.w1 = *w1;
    ::param.w2 = *w2;
    ::param.num_iter = *num_iter;
}

std::shared_ptr<cyHairFile> cmd::exec::smooth(std::shared_ptr<cyHairFile> hairfile) {
    const int hair_count = hairfile->GetHeader().hair_count;

    int offset = 0;
    for (int i = 0; i < hair_count; ++i) {
        if (i > 0 && i % 100 == 0)
            spdlog::debug("Processing hair {}/{}", i, hair_count);

        const int nsegs = hairfile->GetSegmentsArray() ? hairfile->GetSegmentsArray()[i] : hairfile->GetHeader().d_segments;

        if (nsegs < 2)
            continue;

        // Copy point data to Eigen matrix
        MatrixX3d f = Map<Matrix3Xf>(hairfile->GetPointsArray() + 3*offset, 3, nsegs+1).cast<double>().transpose();

        /*
        Energy to be minimized for coordinate function f:
            E(f) = 1/2 Σ_i w0 * (f_i - f_0_i)^2 + w1 * (f_i+1 - f_i)^2 + w2 * (f_i+1 - 2*f_i + f_i-1)^2
                 = 1/2{ w0 * |f - f_0|^2 + w1 * (D f)^2 + w2 * (L f)^2 }
                 = 1/2 f^T (w0*I + w1*(D^T D) + w2*(L^T L)) f + f^T (-w0 * f_0) + const.
                 = 1/2 f^T Q f + f^T B + const.
        */

        SparseMatrix<double> I;
        igl::speye<double>(nsegs+1, I);

        SparseMatrix<double> D(nsegs, nsegs+1);
        std::vector<Triplet<double>> D_triplets(2*nsegs);
        for (int j = 0; j < nsegs; ++j) {
            D_triplets[2*j+0] = {j, j, 1};
            D_triplets[2*j+1] = {j, j+1, -1};
        }
        D.setFromTriplets(D_triplets.begin(), D_triplets.end());

        SparseMatrix<double> L(nsegs-1, nsegs+1);
        std::vector<Triplet<double>> L_triplets(3*(nsegs-1));
        for (int j = 0; j < nsegs-1; ++j) {
            L_triplets[3*j+0] = {j, j, 1};
            L_triplets[3*j+1] = {j, j+1, -2};
            L_triplets[3*j+2] = {j, j+2, 1};
        }
        L.setFromTriplets(L_triplets.begin(), L_triplets.end());

        SparseMatrix<double> Q = ::param.w0*I + ::param.w1*(D.transpose()*D) + ::param.w2*(L.transpose()*L);

        for (unsigned int iter = 0; iter < ::param.num_iter; ++iter) {
            VectorXi b(2);
            MatrixX3d bc(2, 3);
            b << 0, nsegs;
            bc << f.row(0), f.row(nsegs);

            igl::min_quad_with_fixed_data<double> mqwf;
            SparseMatrix<double> Aeq;
            VectorXd Beq;
            MatrixX3d B = -::param.w0*f;
            igl::min_quad_with_fixed_precompute(Q, b, Aeq, true, mqwf);
            igl::min_quad_with_fixed_solve(mqwf,B,bc,Beq,f);
        }
        // Copy resulting point data back to hairfile
        Matrix3Xf f_T = f.transpose().cast<float>();
        std::memcpy(hairfile->GetPointsArray() + 3*offset, f_T.data(), 3*(nsegs+1)*sizeof(float));

        offset += nsegs + 1;
    }
    return hairfile;
}
