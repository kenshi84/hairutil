#include "io.h"

#include <npy.hpp>

std::shared_ptr<cyHairFile> io::load_npy(const std::string &filename) {
    npy::npy_data d = npy::read_npy<float>(filename);
    if (d.shape.size() != 3) {
        throw std::runtime_error(fmt::format("Invalid shape in npy file: expected 3D array, got {}D array", d.shape.size()));
    }
    if (d.shape[2] != 3) {
        throw std::runtime_error(fmt::format("Invalid shape in npy file: expected 3 channels, got {} channels", d.shape[2]));
    }

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();
    hairfile->SetArrays(_CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(d.shape[0]);
    hairfile->SetDefaultSegmentCount(d.shape[1] - 1);
    hairfile->SetPointCount(d.data.size() / 3);
    std::memcpy(hairfile->GetPointsArray(), d.data.data(), d.data.size() * sizeof(float));

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

    npy::npy_data_ptr<float> d;
    d.data_ptr = hairfile->GetPointsArray();
    d.shape = {hair_count, num_segments + 1, 3};
    npy::write_npy<float>(filename, d);
}
