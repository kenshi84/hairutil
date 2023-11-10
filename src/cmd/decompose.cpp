#include "cmd.h"
#include "io.h"

namespace decompose_params {
bool confirm = false;
std::set<int> indices;
}

void cmd::parse::decompose(args::Subparser &parser) {
    args::Flag confirm(parser, "confirm", "Confirm in case of generating huge number of files", {"confirm"});
    args::ValueFlag<std::string> indices(parser, "indices", "Comma-separated list of indices to extract", {"indices"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::decompose;
    decompose_params::confirm = confirm;

    if (indices) {
        std::stringstream ss(*indices);
        std::string token;
        while (std::getline(ss, token, ',')) {
            decompose_params::indices.insert(std::stoi(token));
        }
    }
}

std::shared_ptr<cyHairFile> cmd::exec::decompose(std::shared_ptr<cyHairFile> hairfile_in) {
    const cyHairFile::Header &header = hairfile_in->GetHeader();

    // Confirm generation of huge number of files
    if (header.hair_count > 20000 && !decompose_params::confirm) {
        throw std::runtime_error(fmt::format("Generating {} files. Use --confirm to proceed", header.hair_count));
    }

    const std::string output_dir = fmt::format("{}_decomposed_{}", globals::input_file_wo_ext, globals::output_ext);

    // Overwrite check
    if (!globals::overwrite && std::filesystem::exists(output_dir)) {
        throw std::runtime_error(fmt::format("Output directory already exists: {}\nUse --overwrite to overwrite", output_dir));
    }

    // Create output directory
    std::filesystem::create_directory(output_dir);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned int segment_count = (header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) ? hairfile_in->GetSegmentsArray()[i] : header.d_segments;

        if (!decompose_params::indices.empty() && !decompose_params::indices.count(i)) {
            offset += segment_count + 1;
            continue;
        }

        auto hairfile_out = std::make_shared<cyHairFile>();

        hairfile_out->SetHairCount(1);

        // Figure out number of points on this strand
        hairfile_out->SetPointCount(segment_count + 1);
        hairfile_out->SetDefaultSegmentCount(segment_count);

        // Allocate array
        hairfile_out->SetArrays(header.arrays & (31 - _CY_HAIR_FILE_SEGMENTS_BIT));

        // Copy values
        for (unsigned int j = 0; j <= segment_count; ++j) {
            for (unsigned int k = 0; k < 3; ++k) {
                hairfile_out->GetPointsArray()[3*j + k] = hairfile_in->GetPointsArray()[3*(offset+j) + k];

                if (header.arrays & _CY_HAIR_FILE_COLORS_BIT)
                    hairfile_out->GetColorsArray()[3*j + k] = hairfile_in->GetColorsArray()[3*(offset+j) + k];
            }

            if (header.arrays & _CY_HAIR_FILE_THICKNESS_BIT) hairfile_out->GetThicknessArray()[j] = hairfile_in->GetThicknessArray()[offset + j];
            if (header.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT) hairfile_out->GetTransparencyArray()[j] = hairfile_in->GetTransparencyArray()[offset + j];
        }

        if (!(header.arrays & _CY_HAIR_FILE_THICKNESS_BIT)) hairfile_out->SetDefaultThickness(header.d_thickness);
        if (!(header.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT)) hairfile_out->SetDefaultTransparency(header.d_transparency);
        if (!(header.arrays & _CY_HAIR_FILE_COLORS_BIT)) hairfile_out->SetDefaultColor(header.d_color[0], header.d_color[1], header.d_color[2]);

        // Save
        const std::string output_file = fmt::format("{}/{}.{}", output_dir, i, globals::output_ext);
        if (header.hair_count < 1000 || (i > 0 && i % 1000 == 0) || !decompose_params::indices.empty()) {
            spdlog::info("Saving to {} ...", output_file);
        }
        globals::save_func(output_file, hairfile_out);

        offset += segment_count + 1;
    }

    return {};
}
