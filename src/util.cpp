#include "util.h"

std::shared_ptr<cyHairFile> util::get_subset(std::shared_ptr<cyHairFile> hairfile_in, const std::vector<unsigned char>& selected) {
    const auto& header_in = hairfile_in->GetHeader();

    const unsigned int num_selected = std::accumulate(selected.begin(), selected.end(), 0);

    if (num_selected == 0) {
        spdlog::warn("No strand is selected");
        return {};
    }
    spdlog::info("Selected {} strands", num_selected);

    // Count the number of selected strands and their points
    unsigned int out_hair_count = num_selected;
    unsigned int out_point_count = 0;
    for (int i = 0; i < header_in.hair_count; ++i)
    {
        if (selected[i])
            out_point_count += (hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments) + 1;
    }

    // Create output hair file
    std::shared_ptr<cyHairFile> hairfile_out = std::make_shared<cyHairFile>();

    // Copy header via memcpy
    std::memcpy((void*)&hairfile_out->GetHeader(), &header_in, sizeof(cyHairFile::Header));

    // Set hair_count & point_count, allocate arrays
    hairfile_out->SetHairCount(out_hair_count);
    hairfile_out->SetPointCount(out_point_count);
    hairfile_out->SetArrays(header_in.arrays);

    // Copy array data
    unsigned int in_point_offset = 0;
    unsigned int out_hair_idx = 0;
    unsigned int out_point_offset = 0;
    spdlog::debug("Input-output index mapping (strand idx ; root vertex idx):");
    for (unsigned int in_hair_idx = 0; in_hair_idx < header_in.hair_count; ++in_hair_idx)
    {
        const unsigned int nsegs = hairfile_in->GetSegmentsArray() ? hairfile_in->GetSegmentsArray()[in_hair_idx] : header_in.d_segments;

        if (selected[in_hair_idx])
        {
            spdlog::debug("  {} -> {} ; {} -> {}", in_hair_idx, out_hair_idx, in_point_offset, out_point_offset);

            // Copy segment info if available
            if (header_in.arrays & _CY_HAIR_FILE_SEGMENTS_BIT)
                hairfile_out->GetSegmentsArray()[out_hair_idx++] = nsegs;

            // Copy per-point info if available
            for (int j = 0; j < nsegs + 1; ++j)
            {
                if (header_in.arrays & _CY_HAIR_FILE_POINTS_BIT)
                {
                    for (int k = 0; k < 3; ++k)
                        hairfile_out->GetPointsArray()[3*(out_point_offset + j) + k] = hairfile_in->GetPointsArray()[3*(in_point_offset + j) + k];
                }

                if (header_in.arrays & _CY_HAIR_FILE_THICKNESS_BIT)
                    hairfile_out->GetThicknessArray()[out_point_offset + j] = hairfile_in->GetThicknessArray()[in_point_offset + j];

                if (header_in.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT)
                    hairfile_out->GetTransparencyArray()[out_point_offset + j] = hairfile_in->GetTransparencyArray()[in_point_offset + j];

                if (header_in.arrays & _CY_HAIR_FILE_COLORS_BIT)
                {
                    for (int k = 0; k < 3; ++k)
                        hairfile_out->GetColorsArray()[3*(out_point_offset + j) + k] = hairfile_in->GetColorsArray()[3*(in_point_offset + j) + k];
                }
            }
            out_point_offset += nsegs + 1;
        }
        in_point_offset += nsegs + 1;
    }
    if (header_in.arrays & _CY_HAIR_FILE_SEGMENTS_BIT)
        assert(out_hair_idx == out_hair_count);

    return hairfile_out;
}
