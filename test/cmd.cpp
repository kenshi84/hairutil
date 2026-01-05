#include <gtest/gtest.h>

#include "io.h"

extern int test_main(int argc, const char **argv);

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
    io::save_ply(TEST_DATA_DIR "/autofix_test.ply", generate_test_data());
    std::vector<const char*> args = {
        "test_cmd",
        "autofix",
        "-i", TEST_DATA_DIR "/autofix_test.ply",
        "-o", "ply",
        "--overwrite"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_convert, bin_to_abc) {
    std::vector<const char*> args = {
        "test_cmd",
        "convert",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "abc",
        "--overwrite"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_convert, bin_to_data_ply) {
    std::vector<const char*> args = {
        "test_cmd",
        "convert",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "data,ply",
        "-d", TEST_DATA_DIR "/out",
        "--overwrite"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_convert, bin_to_data_ply_bin) {
    std::vector<const char*> args = {
        "test_cmd",
        "convert",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "data,ply,bin",
        "-d", TEST_DATA_DIR "/out",
        "--overwrite"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_decompose, bin_to_ply_data) {
    std::vector<const char*> args = {
        "test_cmd",
        "decompose",
        "-i", TEST_DATA_DIR "/Bangs_20.bin",
        "-o", "ply,data",
        "--overwrite"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_filter, geq_output_indices) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "length",
        "--geq", "174.96289",
        "--output-indices"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_filter, geq_no_output) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "length",
        "--geq", "174.96289",
        "--no-output"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_filter, geq) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "length",
        "--geq", "174.96289",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_filter, fail_geq_gt) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "length",
        "--geq", "174.96289",
        "--gt", "174.96289",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_filter, fail_bad_key) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "angle",
        "--geq", "174.96289",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_filter, fail_no_threshold) {
    std::vector<const char*> args = {
        "test_cmd",
        "filter",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "-k", "length",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_findpenet, Bangs) {
    std::vector<const char*> args = {
        "test_cmd",
        "findpenet",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-m", TEST_DATA_DIR "/Bangs_head.ply",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_getcurvature, Bangs_100) {
    std::vector<const char*> args = {
        "test_cmd",
        "getcurvature",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_info, ply) {
    std::vector<const char*> args = {
        "test_cmd",
        "info",
        "-i", TEST_DATA_DIR "/Bangs_100_binary.ply",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_info, print_json) {
    std::vector<const char*> args = {
        "test_cmd",
        "info",
        "-i", TEST_DATA_DIR "/Bangs_100_binary.ply",
        "-j",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, bin_to_ply) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--target-segment-length", "2.0",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, linear_subdiv) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--target-segment-length", "2.0",
        "--overwrite",
        "--linear-subdiv",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, catmull_rom_1_pow1) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/catmull_rom_test1.ply",
        "--ply-save-ascii",
        "--target-segment-length", "5.0",
        "--overwrite",
        "--catmull-rom",
        "--cr-power", "1.0",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, catmull_rom_2_pow1) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/catmull_rom_test2.ply",
        "--ply-save-ascii",
        "--target-segment-length", "2.0",
        "--overwrite",
        "--catmull-rom",
        "--cr-power", "1.0",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, catmull_rom_1) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/catmull_rom_test1.ply",
        "--ply-save-ascii",
        "--target-segment-length", "5.0",
        "--overwrite",
        "--catmull-rom",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, catmull_rom_2) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/catmull_rom_test2.ply",
        "--ply-save-ascii",
        "--target-segment-length", "2.0",
        "--overwrite",
        "--catmull-rom",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_resample, conflict_linear_catmull) {
    std::vector<const char*> args = {
        "test_cmd",
        "resample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "--target-segment-length", "5.0",
        "--overwrite",
        "--catmull-rom",
        "--linear-subdiv",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_smooth, Bangs_100_ply) {
    std::vector<const char*> args = {
        "test_cmd",
        "smooth",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_smooth, omit_output_ext) {
    std::vector<const char*> args = {
        "test_cmd",
        "smooth",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_stats, export_raw) {
    std::vector<const char*> args = {
        "test_cmd",
        "stats",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "--export-raw-strand",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_subsample, bin_to_ply) {
    std::vector<const char*> args = {
        "test_cmd",
        "subsample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--target-count", "20",
        "--scale-factor", "0.9",
        "--seed", "0"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_subsample, indices) {
    std::vector<const char*> args = {
        "test_cmd",
        "subsample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--indices", "65,32,4,36,0",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_subsample, indices_single) {
    std::vector<const char*> args = {
        "test_cmd",
        "subsample",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--indices", "10",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_transform, invalid_args) {
    std::vector<const char*> args = {
        "test_cmd",
        "transform",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--scale", "1.2",
        "--translate", "12.3,45.6,78.9",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 1);
}

TEST(cmd_transform, scale) {
    std::vector<const char*> args = {
        "test_cmd",
        "transform",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--scale", "1.2",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_transform, translate) {
    std::vector<const char*> args = {
        "test_cmd",
        "transform",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--translate", "12.3,45.6,78.9",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_transform, rotate) {
    std::vector<const char*> args = {
        "test_cmd",
        "transform",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--rotate",
        "0.407903582,-0.656201959,0.634833455,"
        "0.838385462,0.54454118,0.0241773129,"
        "-0.361558199,0.52237314,0.77227056"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_transform, full) {
    std::vector<const char*> args = {
        "test_cmd",
        "transform",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "-o", "ply",
        "--overwrite",
        "--full",
        "0.407903582,-0.656201959,0.634833455,12.3,"
        "0.838385462,0.54454118,0.0241773129,45.6,"
        "-0.361558199,0.52237314,0.77227056,78.9,"
        "0,0,0,1"
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

TEST(cmd_tubify, Bangs_100) {
    std::vector<const char*> args = {
        "test_cmd",
        "tubify",
        "-i", TEST_DATA_DIR "/Bangs_100.bin",
        "--radius", "1.0",
        "--colored",
        "--capped",
        "--overwrite",
    };
    globals::clear();
    EXPECT_EQ(test_main(args.size(), args.data()), 0);
}

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::trace);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
