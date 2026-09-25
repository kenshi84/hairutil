#include "io.h"

#include <npy/npy.h>

namespace {
template <class Tensor>
void cast_copy(const Tensor& src, float* dst, const std::vector<size_t>& shape) {
    for (size_t i = 0; i < shape[0]; ++i) {
        for (size_t j = 0; j < shape[1]; ++j) {
            for (size_t k = 0; k < 3; ++k) {
                dst[(i * shape[1] + j) * 3 + k] = static_cast<float>(src({i, j, k}));
            }
        }
    }
}
}

std::shared_ptr<cyHairFile> io::load_npy(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error(fmt::format("Failed to open npy file: {}", filename));
    }
    ifs.exceptions(std::ios::failbit | std::ios::badbit);
    std::shared_ptr<npy::header_info> header;
    try {
        header = std::make_shared<npy::header_info>(npy::read_npy_header(ifs));
    } catch (const std::exception& e) {
        throw std::runtime_error(fmt::format("Failed to read npy header from file {}: {}", filename, e.what()));
    }
    const auto& shape = header->shape;
    const auto& dtype = header->dtype;
    if (shape.size() != 3) {
        throw std::runtime_error(fmt::format("Invalid shape in npy file: expected 3D array, got {}D array", shape.size()));
    }
    if (shape[2] != 3) {
        throw std::runtime_error(fmt::format("Invalid shape in npy file: expected 3 channels, got {} channels", shape[2]));
    }
    if (dtype != npy::data_type_t::FLOAT16 && dtype != npy::data_type_t::FLOAT32 && dtype != npy::data_type_t::FLOAT64) {
        throw std::runtime_error(fmt::format("Unsupported data type in npy file: {}", npy::to_dtype(dtype)));
    }

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();
    hairfile->SetArrays(_CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(shape[0]);
    hairfile->SetDefaultSegmentCount(shape[1] - 1);
    hairfile->SetPointCount(shape[0] * shape[1]);
    if (dtype == npy::data_type_t::FLOAT32) {
        auto d = npy::load<float, npy::tensor>(filename);
        if (!header->fortran_order)
            std::memcpy(hairfile->GetPointsArray(), d.data(), d.size() * sizeof(float));
        else
            cast_copy(d, hairfile->GetPointsArray(), shape);
    } else if (dtype == npy::data_type_t::FLOAT64) {
        auto d = npy::load<double, npy::tensor>(filename);
        cast_copy(d, hairfile->GetPointsArray(), shape);
    } else {
        auto d = npy::load<npy::float16_t, npy::tensor>(filename);
        cast_copy(d, hairfile->GetPointsArray(), shape);
    }

    return hairfile;
}

void io::save_npy(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    unsigned int hair_count = hairfile->GetHeader().hair_count;
    unsigned int num_segments;
    if (hairfile->GetSegmentsArray()) {
        num_segments = hairfile->GetSegmentsArray()[0];
        for (unsigned int i = 1; i < hair_count; ++i) {
            if (hairfile->GetSegmentsArray()[i] != num_segments) {
                throw std::runtime_error(fmt::format("Inconsistent segment count: {} vs {} at {}", hairfile->GetSegmentsArray()[i], num_segments, i));
            }
        }
    } else {
        num_segments = hairfile->GetHeader().d_segments;
    }

    if (globals::npy_save_float16) {
        npy::tensor<npy::float16_t> d({hair_count, num_segments + 1, 3});
        for (unsigned int i = 0; i < hair_count; ++i) {
            for (unsigned int j = 0; j < num_segments + 1; ++j) {
                for (unsigned int k = 0; k < 3; ++k) {
                    d({i, j, k}) = static_cast<npy::float16_t>(hairfile->GetPointsArray()[(i * (num_segments + 1) + j) * 3 + k]);
                }
            }
        }
        npy::save(filename, d);
    } else {
        npy::tensor<float> d({hair_count, num_segments + 1, 3});
        std::memcpy(d.data(), hairfile->GetPointsArray(), hair_count * (num_segments + 1) * 3 * sizeof(float));
        npy::save(filename, d);
    }
}
