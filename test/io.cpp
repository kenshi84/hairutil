#include <gtest/gtest.h>

#include "io.h"

namespace {

std::shared_ptr<cyHairFile> generate_test_data() {
    // Prepare data
    const std::vector<unsigned short> segments_array = { 3, 4, 5, 6, 7 };
    const unsigned int hair_count = segments_array.size();

    const unsigned int point_count = std::accumulate(segments_array.begin(), segments_array.end(), hair_count);

    std::vector<float> points_array(point_count * 3);
    std::vector<float> thickness_array(point_count);
    std::vector<float> transparency_array(point_count);
    std::vector<float> colors_array(point_count * 3);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        const unsigned short segments = segments_array[i];
        for (unsigned short j = 0; j <= segments; ++j) {
            points_array[3*(offset+j) + 0] = i;
            points_array[3*(offset+j) + 1] = j;
            points_array[3*(offset+j) + 2] = 0;

            thickness_array[offset+j] = 0.1f * (1 + i) * (1 + j);

            transparency_array[offset+j] = 0.5f;

            colors_array[3*(offset+j) + 0] = (i + 1.0f) / hair_count;
            colors_array[3*(offset+j) + 1] = (j + 1.0f) / segments;
            colors_array[3*(offset+j) + 2] = 0.5f;
        }
        offset += segments + 1;
    }

    // Store in the cyHairFile structure
    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(
        _CY_HAIR_FILE_SEGMENTS_BIT |
        _CY_HAIR_FILE_POINTS_BIT |
        _CY_HAIR_FILE_THICKNESS_BIT |
        _CY_HAIR_FILE_TRANSPARENCY_BIT |
        _CY_HAIR_FILE_COLORS_BIT
    );
    hairfile->SetHairCount(hair_count);
    hairfile->SetPointCount(point_count);

    std::memcpy(hairfile->GetSegmentsArray(), segments_array.data(), sizeof(unsigned short) * hair_count);
    std::memcpy(hairfile->GetPointsArray(), points_array.data(), sizeof(float) * point_count * 3);
    std::memcpy(hairfile->GetThicknessArray(), thickness_array.data(), sizeof(float) * point_count);
    std::memcpy(hairfile->GetTransparencyArray(), transparency_array.data(), sizeof(float) * point_count);
    std::memcpy(hairfile->GetColorsArray(), colors_array.data(), sizeof(float) * point_count * 3);

    hairfile->SetDefaultSegmentCount(10);
    hairfile->SetDefaultThickness(0.1f);
    hairfile->SetDefaultTransparency(0.5f);
    hairfile->SetDefaultColor(0.25f, 0.5f, 0.75f);

    return hairfile;
}

}

TEST(io_abc, read) { auto hairfile = io::load_abc(TEST_DATA_DIR "/Bangs_100.abc"); }
TEST(io_bin, read) { auto hairfile = io::load_bin(TEST_DATA_DIR "/Bangs_100.bin"); }
TEST(io_data, read) { auto hairfile = io::load_data(TEST_DATA_DIR "/Bangs_100.data"); }
TEST(io_hair, read) { auto hairfile = io::load_hair(TEST_DATA_DIR "/Bangs_100.hair"); }
TEST(io_ma, read) { auto hairfile = io::load_ma(TEST_DATA_DIR "/Bangs_100.ma"); }
TEST(io_ply, read_ascii) { auto hairfile = io::load_ply(TEST_DATA_DIR "/Bangs_100_ascii.ply"); }
TEST(io_ply, read_binary) { auto hairfile = io::load_ply(TEST_DATA_DIR "/Bangs_100_binary.ply"); }

TEST(io_abc, write) { auto hairfile = generate_test_data(); io::save_abc("test_io_out.abc", hairfile); }
TEST(io_bin, write) { auto hairfile = generate_test_data(); io::save_bin("test_io_out.bin", hairfile); }
TEST(io_data, write) { auto hairfile = generate_test_data(); io::save_data("test_io_out.data", hairfile); }
TEST(io_hair, write) { auto hairfile = generate_test_data(); io::save_hair("test_io_out.hair", hairfile); }
TEST(io_ma, write) { auto hairfile = generate_test_data(); io::save_ma("test_io_out.ma", hairfile); }
TEST(io_ply, write_ascii) { auto hairfile = generate_test_data(); globals::ply_save_ascii = true; io::save_ply("test_io_out_ascii.ply", hairfile); }
TEST(io_ply, write_binary) { auto hairfile = generate_test_data(); globals::ply_save_ascii = false; io::save_ply("test_io_out_binary.ply", hairfile); }

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
