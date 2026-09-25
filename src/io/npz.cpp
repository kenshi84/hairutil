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

std::shared_ptr<cyHairFile> io::load_npz(const std::string &filename) {
    npy::npzfilereader input(filename);
    const auto& keys = input.keys();
    std::set<std::string> candidate_keys;
    for (const auto& key : keys) {
        const auto& header = input.peek(key);
        const auto& shape = header.shape;
        const auto& dtype = header.dtype;
        if (shape.size() == 3 && shape[0] > 0 && shape[1] > 1 && shape[2] == 3 &&
            (dtype == npy::data_type_t::FLOAT16 || dtype == npy::data_type_t::FLOAT32 || dtype == npy::data_type_t::FLOAT64))
        {
            candidate_keys.insert(key.substr(0, key.find_last_of('.')));
        }
    }
    if (candidate_keys.empty()) {
        throw std::runtime_error("No suitable key found in the NPZ file.");
    }
    std::string key;
    if (candidate_keys.size() == 1) {
        key = *candidate_keys.begin();
        if (key != globals::npz_key && !globals::npz_key.empty()) {
            log_warn("The single suitable key '{}' in the NPZ file does not match the specified key '{}'. Reading it anyway.", key, globals::npz_key);
        }
    } else if (candidate_keys.count(globals::npz_key)) {
        key = globals::npz_key;
    } else {
        std::string candidate_keys_str = *candidate_keys.begin();
        for (auto it = std::next(candidate_keys.begin()); it != candidate_keys.end(); ++it) {
            candidate_keys_str += ", " + *it;
        }
        throw std::runtime_error(fmt::format("Multiple candidate keys exist: {} ; use --npz_key to specify one", candidate_keys_str));
    }
    key = key + ".npy";
    const auto& header = input.peek(key);
    const auto& shape = header.shape;
    const auto& dtype = header.dtype;

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();
    hairfile->SetArrays(_CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(shape[0]);
    hairfile->SetDefaultSegmentCount(shape[1] - 1);
    hairfile->SetPointCount(shape[0] * shape[1]);
    if (dtype == npy::data_type_t::FLOAT32) {
        auto d = input.read<npy::tensor<float>>(key);
        if (!header.fortran_order)
            std::memcpy(hairfile->GetPointsArray(), d.data(), d.size() * sizeof(float));
        else
            cast_copy(d, hairfile->GetPointsArray(), shape);
    } else if (dtype == npy::data_type_t::FLOAT64) {
        auto d = input.read<npy::tensor<double>>(key);
        cast_copy(d, hairfile->GetPointsArray(), shape);
    } else {
        auto d = input.read<npy::tensor<npy::float16_t>>(key);
        cast_copy(d, hairfile->GetPointsArray(), shape);
    }

    return hairfile;
}

void io::save_npz(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
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

    npy::npzfilewriter output(filename, globals::npz_save_compressed ? npy::compression_method_t::DEFLATED : npy::compression_method_t::STORED);
    if (!output.is_open())
        throw std::runtime_error(fmt::format("Failed to open npz file for writing: {}", filename));

        std::string key = globals::npz_key;
    if (key.empty()) {
        key = "point";
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
        output.write(key, d);
    } else {
        npy::tensor<float> d({hair_count, num_segments + 1, 3});
        std::memcpy(d.data(), hairfile->GetPointsArray(), hair_count * (num_segments + 1) * 3 * sizeof(float));
        output.write(key, d);
    }
    output.close();
}
