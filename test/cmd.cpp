#include <filesystem>

#include <gtest/gtest.h>

#include "cmd.h"
#include "io.h"

using namespace Eigen;

namespace {

std::shared_ptr<cyHairFile> generate_test_data() {
    // Prepare data
    const std::vector<unsigned short> segments_array = { 3, 4, 0, 6, 7 };
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

TEST(cmd_autofix, empty_strand) {
    auto hairfile = generate_test_data();
    auto hairfile_fixed = cmd::exec::autofix(hairfile);
    EXPECT_NE(hairfile_fixed, nullptr);
}

TEST(cmd_convert, bin_to_abc) {
    auto hairfile_in = io::load_bin(TEST_DATA_DIR "/Bangs_100.bin"); 
    auto hairfile_out = cmd::exec::convert(hairfile_in);
    io::save_abc("test_cmd_convert_out.abc", hairfile_out);
}

TEST(cmd_decompose, bin_to_ply) {
    std::filesystem::copy(TEST_DATA_DIR "/Bangs_20.bin", "test_cmd_decompose.bin", std::filesystem::copy_options::overwrite_existing);

    auto hairfile_in = io::load_bin("test_cmd_decompose.bin");
    globals::input_file_wo_ext = "test_cmd_decompose";
    globals::output_ext = "ply";
    globals::overwrite = true;
    globals::save_func = io::save_ply;

    cmd::exec::decompose(hairfile_in);
}

TEST(cmd_info, ply) {
    auto hairfile = io::load_ply(TEST_DATA_DIR "/Bangs_100_binary.ply");
    cmd::exec::info(hairfile);
}

TEST(cmd_resample, bin_to_ply) {
    auto hairfile_in = io::load_bin(TEST_DATA_DIR "/Bangs_20.bin");
    auto hairfile_out = cmd::exec::resample(hairfile_in);
    io::save_ply("test_cmd_resample_out.ply", hairfile_out);
}

namespace susbample_params {
extern unsigned int target_count;
extern float scale_factor;
}

TEST(cmd_subsample, bin_to_ply) {
    auto hairfile_in = io::load_bin(TEST_DATA_DIR "/Bangs_100.bin");

    globals::rng.seed(0);
    susbample_params::target_count = 20;
    susbample_params::scale_factor = 0.9;
    auto hairfile_out = cmd::exec::subsample(hairfile_in);

    io::save_ply("test_cmd_subsample_out.ply", hairfile_out);
}

namespace transform_params {
extern float s;
extern Vector3f t;
extern Matrix3f R;
}
TEST(cmd_transform, bin_to_ply) {
    auto hairfile_in = io::load_bin(TEST_DATA_DIR "/Bangs_100.bin");

    transform_params::s = 1.2;
    transform_params::t << 12.3, 45.6, 78.9;
    transform_params::R = AngleAxisf(1.2, Vector3f(1, 2, 3).normalized()).toRotationMatrix();

    auto hairfile_out = cmd::exec::transform(hairfile_in);

    io::save_ply("test_cmd_transform_out.ply", hairfile_out);
}

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::trace);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
