/*
File format used in:
    CT2Hair: High-Fidelity 3D Hair Modeling using Computed Tomography (SIGGRAPH 2023)
    https://github.com/Jhonve/HairStrandsRendering/blob/f4ce2c5db4a5ab24a144cbc3297fbc073f83cf29/HairStrandsRendering/elems/strands.cpp#L255
*/

#include "io.h"

std::shared_ptr<cyHairFile> io::load_bin(const std::string &filename) {
    std::ifstream ifs;
    ifs.open(filename.c_str(), std::ios_base::binary);

    if (!ifs.is_open()) {
        throw std::runtime_error(fmt::format("Cannot open file {}", filename));
    }

    // Read the number of strands
    int hair_count;
    if (!ifs.read((char*)&hair_count, sizeof(int))) {
        throw std::runtime_error(fmt::format("Failed to read strand count from BIN file {}", filename));
    }
    if (hair_count < 0) {
        throw std::runtime_error(fmt::format("Invalid strand count {} in BIN file {}", hair_count, filename));
    }

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(_CY_HAIR_FILE_SEGMENTS_BIT | _CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(hair_count);

    std::vector<float> points_array;
    points_array.reserve(static_cast<size_t>(hair_count) * 3 * 1000);

    for (int hair_idx = 0; hair_idx < hair_count; ++hair_idx) {
        if (hair_idx > 0 && hair_idx % 100 == 0)
            spdlog::trace("Processing hair {}/{}", hair_idx, hair_count);

        // Read the number of points in the strand
        int num_points;
        if (!ifs.read((char*)&num_points, sizeof(int))) {
            throw std::runtime_error(fmt::format("Failed to read point count for strand {} from BIN file {}", hair_idx, filename));
        }
        if (num_points <= 0 || num_points > 0x10000) {
            throw std::runtime_error(fmt::format("Invalid point count {} for strand {} in BIN file {}", num_points, hair_idx, filename));
        }

        hairfile->GetSegmentsArray()[hair_idx] = num_points - 1;

        // Read individual points
        for (int j = 0; j < num_points; ++j) {
            // Each record contains XYZ followed by four unused floats.
            float point[7];
            if (!ifs.read((char*)point, sizeof(point))) {
                throw std::runtime_error(fmt::format("Failed to read point {} of strand {} from BIN file {}", j, hair_idx, filename));
            }
            points_array.insert(points_array.end(), point, point + 3);
        }
    }

    // Copy content of points_array to hairfile->GetPointsArray()
    hairfile->SetPointCount(points_array.size() / 3);
    std::memcpy(hairfile->GetPointsArray(), points_array.data(), points_array.size() * sizeof(float));

    return hairfile;
}

void io::save_bin(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    std::ofstream ofs;
    ofs.open(filename.c_str(), std::ios::out | std::ios::binary);

    if (!ofs.is_open()) {
        throw std::runtime_error(fmt::format("Cannot open file {}", filename));
    }

    const auto& header = hairfile->GetHeader();

    // Write the number of strands
    ofs.write((char*)&header.hair_count, sizeof(int));

    float* points_ptr = hairfile->GetPointsArray();

    for (int hair_idx = 0; hair_idx < header.hair_count; ++hair_idx) {
        if (hair_idx > 0 && hair_idx % 100 == 0)
            spdlog::trace("Processing hair {}/{}", hair_idx, header.hair_count);

        // Write the number of points in the strand
        int num_points = (hairfile->GetSegmentsArray() ? hairfile->GetSegmentsArray()[hair_idx] : header.d_segments) + 1;
        ofs.write((char*)&num_points, sizeof(int));

        // Write individual points
        for (int j = 0; j < num_points; j++) {
            const float x = *points_ptr++;
            const float y = *points_ptr++;
            const float z = *points_ptr++;
            ofs.write((char*)&x, sizeof(float));
            ofs.write((char*)&y, sizeof(float));
            ofs.write((char*)&z, sizeof(float));

            const float dummy = 0;
            for (int k = 0; k < 4; ++k)
                ofs.write((char*)&dummy, sizeof(float));
        }
    }
}
