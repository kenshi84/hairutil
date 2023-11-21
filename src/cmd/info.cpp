#include "cmd.h"

void cmd::parse::info(args::Subparser &parser) {
    parser.Parse();
    globals::cmd_exec = cmd::exec::info;
}

std::shared_ptr<cyHairFile> cmd::exec::info(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header = hairfile_in->GetHeader();
    spdlog::info("================================================================");
    spdlog::info("Segments array: {}", header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? "Yes" : "No");
    spdlog::info("Points array: {}", header.arrays & _CY_HAIR_FILE_POINTS_BIT ? "Yes" : "No");
    spdlog::info("Thickness array: {}", header.arrays & _CY_HAIR_FILE_THICKNESS_BIT ? "Yes" : "No");
    spdlog::info("Transparency array: {}", header.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT ? "Yes" : "No");
    spdlog::info("Colors array: {}", header.arrays & _CY_HAIR_FILE_COLORS_BIT ? "Yes" : "No");
    if ((header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) == 0) spdlog::info("Default segments: {}", header.d_segments);
    if ((header.arrays & _CY_HAIR_FILE_THICKNESS_BIT) == 0) spdlog::info("Default thickness: {}", header.d_thickness);
    if ((header.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT) == 0) spdlog::info("Default transparency: {}", header.d_transparency);
    if ((header.arrays & _CY_HAIR_FILE_COLORS_BIT) == 0) spdlog::info("Default color: ({}, {}, {})", header.d_color[0], header.d_color[1], header.d_color[2]);
    spdlog::info("================================================================");
    return {};
}
