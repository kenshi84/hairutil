#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {

unsigned int stats_sort_size;

struct StrandInfo {
    size_t idx;
    unsigned int nsegs;
    float length;
};
struct SegmentInfo {
    size_t idx;
    unsigned int strand_idx;
    unsigned int local_idx;         // Index of the segment within the strand
    float length;
};
struct PointInfo {
    size_t idx;                   // Index of the center point into the global points array
    unsigned int strand_idx;
    unsigned int local_idx;             // Index of the center point within the strand points
    float circumradius_reciprocal;      // Reciprocal of the circumradius of the wedge triangle formed by the point and its two neighbors
    float turning_angle;
};

}

void cmd::parse::info(args::Subparser &parser) {
    args::ValueFlag<unsigned int> stats_sort_size(parser, "N", "Print top-N sorted list of items in stats [10]", {"stats-sort-size"}, 10);
    parser.Parse();
    globals::cmd_exec = cmd::exec::info;
    ::stats_sort_size = *stats_sort_size;
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

    // Report statistics when debug mode
    if (spdlog::get_level() <= spdlog::level::debug) {
        if (::stats_sort_size > header.hair_count) {
            throw std::runtime_error(fmt::format("stats-sort-size ({}) > hair_count ({})", ::stats_sort_size, header.hair_count));
        }

        spdlog::debug("Statistics:");

        std::vector<StrandInfo> strand_info;    strand_info.reserve(header.hair_count);
        std::vector<SegmentInfo> segment_info;  segment_info.reserve(header.point_count);
        std::vector<PointInfo> point_info;      point_info.reserve(header.point_count);

        unsigned int offset = 0;
        for (unsigned int i = 0; i < header.hair_count; ++i) {
            const unsigned int nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile_in->GetSegmentsArray()[i] : header.d_segments;

            point_info.push_back({
                point_info.size(),
                i,      // Strand index
                0,      // Strand-local index
                0.0f,   // Circumradius reciprocal
                0.0f    // Turning angle
            });

            float strand_length = 0.0f;
            Vector3f prev_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * offset);
            for (unsigned int j = 0; j < nsegs; ++j) {
                const Vector3f point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
                const float segment_length = (point - prev_point).norm();

                segment_info.push_back({
                    segment_info.size(),
                    i,                  // Strand index
                    j,                  // Strand-local index
                    segment_length
                });

                if (j < nsegs - 1) {
                    // Compute the circumradius of the wedge triangle
                    const Vector3f next_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 2));
                    const float la = (point - prev_point).norm();
                    const float lb = (next_point - point).norm();
                    const float lc = (next_point - prev_point).norm();
                    const float s = (la + lb + lc) / 2.0f;
                    const float A = std::sqrt(s * (s - la) * (s - lb) * (s - lc));
                    const float circumradius_reciprocal = A > 0.0f ? 1.0f / (la * lb * lc / (4.0f * A)) : 0.0f;
                    const float turning_angle = 180.0f - std::acos(std::clamp((la * la + lb * lb - lc * lc) / (2.0f * la * lb), -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float>;

                    point_info.push_back({
                        point_info.size(),
                        i,                      // Strand index
                        j + 1,                  // Strand-local index
                        circumradius_reciprocal,
                        turning_angle
                    });
                }

                strand_length += segment_length;
                prev_point = point;
            }

            // Insert dummy wedge info at the strand end
            point_info.push_back({
                point_info.size(), 
                i,      // Strand index
                nsegs,  // Strand-local index
                0.0f,   // Circumradius reciprocal
                0.0f    // Turning angle
            });

            strand_info.push_back({
                strand_info.size(),
                nsegs,
                strand_length
            });

            offset += nsegs + 1;
            assert(offset == point_info.size());
        }

        spdlog::debug("Strand stats:");
        const auto& strand_length_stats = util::get_stats(strand_info, [](const auto& a) { return a.length; }, ::stats_sort_size);
        spdlog::debug("  [length]");
        spdlog::debug("    min: [{}] {}", strand_length_stats.min.idx, strand_length_stats.min.length);
        spdlog::debug("    max: [{}] {}", strand_length_stats.max.idx, strand_length_stats.max.length);
        spdlog::debug("    median: [{}] {}", strand_length_stats.median.idx, strand_length_stats.median.length);
        spdlog::debug("    average: {}", strand_length_stats.average);
        if (::stats_sort_size > 0) {
            spdlog::debug("    [top {} largest]", ::stats_sort_size);
            for (const auto& i : strand_length_stats.largest) spdlog::debug("      [{}] {}", i.idx, i.length);
            spdlog::debug("    [top {} smallest]", ::stats_sort_size);
            for (const auto& i : strand_length_stats.smallest) spdlog::debug("      [{}] {}", i.idx, i.length);
        }
        const auto& strand_nsegs_stats = util::get_stats(strand_info, [](const auto& a) { return a.nsegs; }, ::stats_sort_size);
        spdlog::debug("  [nsegs]");
        spdlog::debug("    min: [{}] {}", strand_nsegs_stats.min.idx, strand_nsegs_stats.min.nsegs);
        spdlog::debug("    max: [{}] {}", strand_nsegs_stats.max.idx, strand_nsegs_stats.max.nsegs);
        spdlog::debug("    median: [{}] {}", strand_nsegs_stats.median.idx, strand_nsegs_stats.median.nsegs);
        spdlog::debug("    average: {}", strand_nsegs_stats.average);
        if (::stats_sort_size > 0) {
            spdlog::debug("    [top {} largest]", ::stats_sort_size);
            for (const auto& i : strand_nsegs_stats.largest) spdlog::debug("      [{}] {}", i.idx, i.nsegs);
            spdlog::debug("    [top {} smallest]", ::stats_sort_size);
            for (const auto& i : strand_nsegs_stats.smallest) spdlog::debug("      [{}] {}", i.idx, i.nsegs);
        }

        spdlog::debug("Segment stats:");
        const auto& segment_stats = util::get_stats(segment_info, [](const auto& a) { return a.length; }, ::stats_sort_size);
        spdlog::debug("  [length]");
        spdlog::debug("         [idx/strand_idx/local_idx]");
        spdlog::debug("    min: [{}/{}/{}] {}", segment_stats.min.idx, segment_stats.min.strand_idx, segment_stats.min.local_idx, segment_stats.min.length);
        spdlog::debug("    max: [{}/{}/{}] {}", segment_stats.max.idx, segment_stats.max.strand_idx, segment_stats.max.local_idx, segment_stats.max.length);
        spdlog::debug("    median: [{}/{}/{}] {}", segment_stats.median.idx, segment_stats.median.strand_idx, segment_stats.median.local_idx, segment_stats.median.length);
        spdlog::debug("    average: {}", segment_stats.average);
        if (::stats_sort_size > 0) {
            spdlog::debug("    [top {} largest]", ::stats_sort_size);
            for (const auto& i : segment_stats.largest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.length);
            spdlog::debug("    [top {} smallest]", ::stats_sort_size);
            for (const auto& i : segment_stats.smallest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.length);
        }

        spdlog::debug("Point stats:");
        const auto& point_crr_stats = util::get_stats(point_info, [](const auto& a) { return a.circumradius_reciprocal; }, ::stats_sort_size);
        spdlog::debug("  [circumradius reciprocal]");
        spdlog::debug("         [idx/strand_idx/local_idx]");
        spdlog::debug("    min: [{}/{}/{}] {}", point_crr_stats.min.idx, point_crr_stats.min.strand_idx, point_crr_stats.min.local_idx, point_crr_stats.min.circumradius_reciprocal);
        spdlog::debug("    max: [{}/{}/{}] {}", point_crr_stats.max.idx, point_crr_stats.max.strand_idx, point_crr_stats.max.local_idx, point_crr_stats.max.circumradius_reciprocal);
        spdlog::debug("    median: [{}/{}/{}] {}", point_crr_stats.median.idx, point_crr_stats.median.strand_idx, point_crr_stats.median.local_idx, point_crr_stats.median.circumradius_reciprocal);
        spdlog::debug("    average: {}", point_crr_stats.average);
        if (::stats_sort_size > 0) {
            spdlog::debug("    [top {} largest]", ::stats_sort_size);
            for (const auto& i : point_crr_stats.largest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.circumradius_reciprocal);
            spdlog::debug("    [top {} smallest]", ::stats_sort_size);
            for (const auto& i : point_crr_stats.smallest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.circumradius_reciprocal);
        }
        const auto& point_ta_stats = util::get_stats(point_info, [](const auto& a) { return a.turning_angle; }, ::stats_sort_size);
        spdlog::debug("  [turning angle (degree)]");
        spdlog::debug("         [idx/strand_idx/local_idx]");
        spdlog::debug("    min: [{}/{}/{}] {}", point_ta_stats.min.idx, point_ta_stats.min.strand_idx, point_ta_stats.min.local_idx, point_ta_stats.min.turning_angle);
        spdlog::debug("    max: [{}/{}/{}] {}", point_ta_stats.max.idx, point_ta_stats.max.strand_idx, point_ta_stats.max.local_idx, point_ta_stats.max.turning_angle);
        spdlog::debug("    median: [{}/{}/{}] {}", point_ta_stats.median.idx, point_ta_stats.median.strand_idx, point_ta_stats.median.local_idx, point_ta_stats.median.turning_angle);
        spdlog::debug("    average: {}", point_ta_stats.average);
        if (::stats_sort_size > 0) {
            spdlog::debug("    [top {} largest]", ::stats_sort_size);
            for (const auto& i : point_ta_stats.largest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.turning_angle);
            spdlog::debug("    [top {} smallest]", ::stats_sort_size);
            for (const auto& i : point_ta_stats.smallest) spdlog::debug("      [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.turning_angle);
        }
    }

    return {};
}
