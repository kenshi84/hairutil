/*
File format used in:
    Hair Photobooth: Geometric and Photometric Acquisition of Real Hairstyles (SIGGRAPH 2008)
    https://web.archive.org/web/20180118090113/http://graphics.ucsd.edu/~will/download/HairPhotobooth/

    Single-View Hair Modeling Using A Hairstyle Database (SIGGRAPH 2015)
    https://web.archive.org/web/20230323054045/http://www-scf.usc.edu/~liwenhu/SHM/database.html
*/

#include "io.h"

std::shared_ptr<cyHairFile> io::load_data(const std::string &filename) {
    std::ifstream ifs;
    ifs.open(filename.c_str(), std::ios_base::binary);

    if (!ifs.is_open()) {
        throw std::runtime_error(fmt::format("Cannot open file {}", filename));
    }

    // Read the number of strands
    int hair_count;
    ifs.read((char*)&hair_count, sizeof(int));

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(_CY_HAIR_FILE_SEGMENTS_BIT | _CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(hair_count);

    std::vector<float> points_array;
    points_array.reserve(hair_count * 3 * 1000);

    for (int hair_idx = 0; hair_idx < hair_count; ++hair_idx) {
        if (hair_idx > 0 && hair_idx % 100 == 0)
            spdlog::trace("Processing hair {}/{}", hair_idx, hair_count);

        // Read the number of points in the strand
        int num_points;
        ifs.read((char*)&num_points, sizeof(int));
        assert(num_points < 0x10000);

        hairfile->GetSegmentsArray()[hair_idx] = num_points - 1;

        // Read individual points
        for (int j = 0; j < num_points; ++j) {
            float x, y, z;
            ifs.read((char*)&x, sizeof(float));     points_array.push_back(x);
            ifs.read((char*)&y, sizeof(float));     points_array.push_back(y);
            ifs.read((char*)&z, sizeof(float));     points_array.push_back(z);
        }
    }

    // Copy content of points_array to hairfile->GetPointsArray()
    hairfile->SetPointCount(points_array.size() / 3);
    std::memcpy(hairfile->GetPointsArray(), points_array.data(), points_array.size() * sizeof(float));

    return hairfile;
}

void io::save_data(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
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
        }
    }
}
