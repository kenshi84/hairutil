#include <filesystem>

#include <gtest/gtest.h>

#include "cmd.h"
#include "io.h"

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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
