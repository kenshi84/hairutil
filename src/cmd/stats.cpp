#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {

struct {
    unsigned int sort_size;
    bool export_csv;
    bool no_print;
} param;

struct StrandInfo {
    size_t idx = 0;
    unsigned int nsegs = 0;
    float length = 0;
    float turning_angle_sum = 0;
    float max_segment_length = 0;
    float min_segment_length = std::numeric_limits<float>::max();
    float max_segment_turning_angle_diff = 0;
    float min_segment_turning_angle_diff = std::numeric_limits<float>::max();
    float max_point_circumradius_reciprocal = 0;
    float min_point_circumradius_reciprocal = std::numeric_limits<float>::max();
    float max_point_turning_angle = 0;
    float min_point_turning_angle = std::numeric_limits<float>::max();
};
struct SegmentInfo {
    size_t idx = 0;
    unsigned int strand_idx = 0;
    unsigned int local_idx = 0;         // Index of the segment within the strand
    float length = 0;
    float turning_angle_diff = 0;
};
struct PointInfo {
    size_t idx = 0;                   // Index of the center point into the global points array
    unsigned int strand_idx = 0;
    unsigned int local_idx = 0;             // Index of the center point within the strand points
    float circumradius_reciprocal = 0;      // Reciprocal of the circumradius of the wedge triangle formed by the point and its two neighbors
    float turning_angle = 0;
};

}

void cmd::parse::stats(args::Subparser &parser) {
    args::ValueFlag<unsigned int> sort_size(parser, "N", "Print top-N sorted list of items [10]", {"sort-size"}, 10);
    args::Flag export_csv(parser, "export-csv", "Export raw data tables as CSV files", {"export-csv"});
    args::Flag no_print(parser, "no-print", "Do not print the stats", {"no-print"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::stats;
    ::param = {};
    ::param.sort_size = *sort_size;
    ::param.export_csv = export_csv;
    ::param.no_print = no_print;
}

std::shared_ptr<cyHairFile> cmd::exec::stats(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header = hairfile_in->GetHeader();

    std::vector<StrandInfo> strand_info_vec;    strand_info_vec.reserve(header.hair_count);
    std::vector<SegmentInfo> segment_info_vec;  segment_info_vec.reserve(header.point_count);
    std::vector<PointInfo> point_info_vec;      point_info_vec.reserve(header.point_count);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned int nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile_in->GetSegmentsArray()[i] : header.d_segments;

        StrandInfo strand_info;
        strand_info.idx = strand_info_vec.size();
        strand_info.nsegs = nsegs;

        Vector3f prev_point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * offset);
        float prev_turning_angle;
        for (unsigned int j = 0; j < nsegs; ++j) {
            const Vector3f point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + j + 1));
            const float segment_length = (point - prev_point).norm();

            SegmentInfo segment_info;
            segment_info.idx = segment_info_vec.size();
            segment_info.strand_idx = i;
            segment_info.local_idx = j;
            segment_info.length = segment_length;

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

                PointInfo point_info;
                point_info.idx = offset + j + 1;
                point_info.strand_idx = i;
                point_info.local_idx = j + 1;
                point_info.circumradius_reciprocal = circumradius_reciprocal;
                point_info.turning_angle = turning_angle;
                point_info_vec.push_back(point_info);

                if (j > 0) {
                    segment_info.turning_angle_diff = std::abs(turning_angle - prev_turning_angle);

                    strand_info.max_segment_turning_angle_diff = std::max(strand_info.max_segment_turning_angle_diff, segment_info.turning_angle_diff);
                    strand_info.min_segment_turning_angle_diff = std::min(strand_info.min_segment_turning_angle_diff, segment_info.turning_angle_diff);
                }
                prev_turning_angle = turning_angle;

                strand_info.turning_angle_sum += turning_angle;

                strand_info.max_point_circumradius_reciprocal = std::max(strand_info.max_point_circumradius_reciprocal, circumradius_reciprocal);
                strand_info.min_point_circumradius_reciprocal = std::min(strand_info.min_point_circumradius_reciprocal, circumradius_reciprocal);
                strand_info.max_point_turning_angle = std::max(strand_info.max_point_turning_angle, turning_angle);
                strand_info.min_point_turning_angle = std::min(strand_info.min_point_turning_angle, turning_angle);
            }
            segment_info_vec.push_back(segment_info);

            strand_info.length += segment_length;
            strand_info.max_segment_length = std::max(strand_info.max_segment_length, segment_length);
            strand_info.min_segment_length = std::min(strand_info.min_segment_length, segment_length);
            prev_point = point;
        }

        strand_info_vec.push_back(strand_info);

        offset += nsegs + 1;
    }

    if (::param.export_csv) {
        std::ofstream ofs;

        ofs.open(globals::input_file_wo_ext + "_stats_strand.csv");
        ofs << fmt::format("{},{},{},{},{},{},{},{},{},{},{},{}\n",
            "idx",
            "nsegs",
            "length",
            "turning_angle_sum",
            "max_segment_length",
            "min_segment_length",
            "max_segment_turning_angle_diff",
            "min_segment_turning_angle_diff",
            "max_point_circumradius_reciprocal",
            "min_point_circumradius_reciprocal",
            "max_point_turning_angle",
            "min_point_turning_angle"
        );
        for (const auto& i : strand_info_vec)
            ofs << fmt::format("{},{},{},{},{},{},{},{},{},{},{},{}\n",
                i.idx,
                i.nsegs,
                i.length,
                i.turning_angle_sum,
                i.max_segment_length,
                i.min_segment_length,
                i.max_segment_turning_angle_diff,
                i.min_segment_turning_angle_diff,
                i.max_point_circumradius_reciprocal,
                i.min_point_circumradius_reciprocal,
                i.max_point_turning_angle,
                i.min_point_turning_angle
            );
        ofs.close();

        ofs.open(globals::input_file_wo_ext + "_stats_segment.csv");
        ofs << fmt::format("{},{},{},{},{}\n",
            "idx",
            "strand_idx",
            "local_idx",
            "length",
            "turning_angle_diff"
        );
        for (const auto& i : segment_info_vec)
            ofs << fmt::format("{},{},{},{},{}\n",
                i.idx,
                i.strand_idx,
                i.local_idx,
                i.length,
                i.turning_angle_diff
            );
        ofs.close();

        ofs.open(globals::input_file_wo_ext + "_stats_point.csv");
        ofs << fmt::format("{},{},{},{},{}\n",
            "idx",
            "strand_idx",
            "local_idx",
            "circumradius_reciprocal",
            "turning_angle"
        );
        for (const auto& i : point_info_vec)
            ofs << fmt::format("{},{},{},{},{}\n",
                i.idx,
                i.strand_idx,
                i.local_idx,
                i.circumradius_reciprocal,
                i.turning_angle
            );
        ofs.close();

        spdlog::info("Exported raw data tables to {}_stats_*.csv", globals::input_file_wo_ext);
    }

    if (::param.no_print)
        return {};

#define PRINT_STRAND_STATS(KEY) \
    const auto& strand_##KEY##_stats = util::get_stats(strand_info_vec, [](const auto& a) { return a.KEY; }, ::param.sort_size); \
    spdlog::info("*** " #KEY " ***"); \
    spdlog::info("  min: [{}] {}", strand_##KEY##_stats.min.idx, strand_##KEY##_stats.min.KEY); \
    spdlog::info("  max: [{}] {}", strand_##KEY##_stats.max.idx, strand_##KEY##_stats.max.KEY); \
    spdlog::info("  median: [{}] {}", strand_##KEY##_stats.median.idx, strand_##KEY##_stats.median.KEY); \
    spdlog::info("  average (stddev): {} ({})", strand_##KEY##_stats.average, strand_##KEY##_stats.stddev); \
    if (::param.sort_size > 0) { \
        const size_t n = strand_##KEY##_stats.largest.size(); \
        spdlog::info("  top {} largest:", n); \
        for (const auto& i : strand_##KEY##_stats.largest) spdlog::info("    [{}] {}", i.idx, i.KEY); \
        spdlog::info("  top {} smallest:", n); \
        for (const auto& i : strand_##KEY##_stats.smallest) spdlog::info("    [{}] {}", i.idx, i.KEY); \
    }

#define PRINT_OTHER_STATS(NAME, KEY) \
    const auto& NAME##_##KEY##_stats = util::get_stats(NAME##_info_vec, [](const auto& a) { return a.KEY; }, ::param.sort_size); \
    spdlog::info("*** " #KEY " ***"); \
    spdlog::info("       [idx/strand_idx/local_idx]"); \
    spdlog::info("  min: [{}/{}/{}] {}", NAME##_##KEY##_stats.min.idx, NAME##_##KEY##_stats.min.strand_idx, NAME##_##KEY##_stats.min.local_idx, NAME##_##KEY##_stats.min.KEY); \
    spdlog::info("  max: [{}/{}/{}] {}", NAME##_##KEY##_stats.max.idx, NAME##_##KEY##_stats.max.strand_idx, NAME##_##KEY##_stats.max.local_idx, NAME##_##KEY##_stats.max.KEY); \
    spdlog::info("  median: [{}/{}/{}] {}", NAME##_##KEY##_stats.median.idx, NAME##_##KEY##_stats.median.strand_idx, NAME##_##KEY##_stats.median.local_idx, NAME##_##KEY##_stats.median.KEY); \
    spdlog::info("  average (stddev): {} ({})", NAME##_##KEY##_stats.average, NAME##_##KEY##_stats.stddev); \
    if (::param.sort_size > 0) { \
        const size_t n = NAME##_##KEY##_stats.largest.size(); \
        spdlog::info("  top {} largest:", n); \
        for (const auto& i : NAME##_##KEY##_stats.largest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.KEY); \
        spdlog::info("  top {} smallest:", n); \
        for (const auto& i : NAME##_##KEY##_stats.smallest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.KEY); \
    }

    spdlog::info("================================================================");
    spdlog::info("Strand stats:");
    spdlog::info("================================================================");
    PRINT_STRAND_STATS(length);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(nsegs);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(turning_angle_sum);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(max_segment_length);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(min_segment_length);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(max_point_circumradius_reciprocal);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(min_point_circumradius_reciprocal);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(max_point_turning_angle);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(min_point_turning_angle);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(max_segment_turning_angle_diff);
    spdlog::info("----------------------------------------------------------------");
    PRINT_STRAND_STATS(min_segment_turning_angle_diff);

    spdlog::info("================================================================");
    spdlog::info("Segment stats:");
    spdlog::info("================================================================");
    PRINT_OTHER_STATS(segment, length);
    spdlog::info("----------------------------------------------------------------");
    PRINT_OTHER_STATS(segment, turning_angle_diff);

    spdlog::info("================================================================");
    spdlog::info("Point stats:");
    spdlog::info("================================================================");
    PRINT_OTHER_STATS(point, circumradius_reciprocal);
    spdlog::info("----------------------------------------------------------------");
    PRINT_OTHER_STATS(point, turning_angle);

    return {};
}
