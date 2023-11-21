#include "cmd.h"
#include "util.h"

using namespace Eigen;

namespace {

unsigned int sort_size;
bool export_csv;

struct StrandInfo {
    size_t idx;
    unsigned int nsegs;
    float length;
    float turning_angle_sum;
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

void cmd::parse::stats(args::Subparser &parser) {
    args::ValueFlag<unsigned int> sort_size(parser, "N", "Print top-N sorted list of items [10]", {"sort-size"}, 10);
    args::Flag export_csv(parser, "export-csv", "Export raw data tables as CSV files", {"export-csv"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::stats;
    ::sort_size = *sort_size;
    ::export_csv = export_csv;
}

std::shared_ptr<cyHairFile> cmd::exec::stats(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header = hairfile_in->GetHeader();

    if (::sort_size > header.hair_count) {
        throw std::runtime_error(fmt::format("sort_size ({}) > hair_count ({})", ::sort_size, header.hair_count));
    }

    std::vector<StrandInfo> strand_info;    strand_info.reserve(header.hair_count);
    std::vector<SegmentInfo> segment_info;  segment_info.reserve(header.point_count);
    std::vector<PointInfo> point_info;      point_info.reserve(header.point_count);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned int nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile_in->GetSegmentsArray()[i] : header.d_segments;

        float strand_length = 0.0f;
        float turning_angle_sum = 0.0f;
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
                    offset + j + 1,         // Index of the center point into the global points array
                    i,                      // Strand index
                    j + 1,                  // Strand-local index
                    circumradius_reciprocal,
                    turning_angle
                });

                turning_angle_sum += turning_angle;
            }

            strand_length += segment_length;
            prev_point = point;
        }

        strand_info.push_back({
            strand_info.size(),
            nsegs,
            strand_length,
            turning_angle_sum
        });

        offset += nsegs + 1;
    }

    if (::export_csv) {
        std::ofstream ofs;

        ofs.open(globals::input_file_wo_ext + "_stats_strand.csv");
        ofs << "idx,nsegs,length,turning_angle_sum\n";
        for (const auto& i : strand_info) ofs << i.idx << "," << i.nsegs << "," << i.length << "," << i.turning_angle_sum << "\n";
        ofs.close();

        ofs.open(globals::input_file_wo_ext + "_stats_segment.csv");
        ofs << "idx,strand_idx,local_idx,length\n";
        for (const auto& i : segment_info) ofs << i.idx << "," << i.strand_idx << "," << i.local_idx << "," << i.length << "\n";
        ofs.close();

        ofs.open(globals::input_file_wo_ext + "_stats_point.csv");
        ofs << "idx,strand_idx,local_idx,circumradius_reciprocal,turning_angle\n";
        for (const auto& i : point_info) ofs << i.idx << "," << i.strand_idx << "," << i.local_idx << "," << i.circumradius_reciprocal << "," << i.turning_angle << "\n";
        ofs.close();

        spdlog::info("Exported raw data tables to {}_stats_*.csv", globals::input_file_wo_ext);
    }

    spdlog::info("================================================================");
    spdlog::info("Strand stats:");
    spdlog::info("================================================================");
    const auto& strand_length_stats = util::get_stats(strand_info, [](const auto& a) { return a.length; }, ::sort_size);
    spdlog::info("*** length ***");
    spdlog::info("  min: [{}] {}", strand_length_stats.min.idx, strand_length_stats.min.length);
    spdlog::info("  max: [{}] {}", strand_length_stats.max.idx, strand_length_stats.max.length);
    spdlog::info("  median: [{}] {}", strand_length_stats.median.idx, strand_length_stats.median.length);
    spdlog::info("  average: {}", strand_length_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : strand_length_stats.largest) spdlog::info("    [{}] {}", i.idx, i.length);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : strand_length_stats.smallest) spdlog::info("    [{}] {}", i.idx, i.length);
    }
    spdlog::info("----------------------------------------------------------------");
    const auto& strand_nsegs_stats = util::get_stats(strand_info, [](const auto& a) { return a.nsegs; }, ::sort_size);
    spdlog::info("*** nsegs ***");
    spdlog::info("  min: [{}] {}", strand_nsegs_stats.min.idx, strand_nsegs_stats.min.nsegs);
    spdlog::info("  max: [{}] {}", strand_nsegs_stats.max.idx, strand_nsegs_stats.max.nsegs);
    spdlog::info("  median: [{}] {}", strand_nsegs_stats.median.idx, strand_nsegs_stats.median.nsegs);
    spdlog::info("  average: {}", strand_nsegs_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : strand_nsegs_stats.largest) spdlog::info("    [{}] {}", i.idx, i.nsegs);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : strand_nsegs_stats.smallest) spdlog::info("    [{}] {}", i.idx, i.nsegs);
    }
    spdlog::info("----------------------------------------------------------------");
    const auto& strand_tas_stats = util::get_stats(strand_info, [](const auto& a) { return a.turning_angle_sum; }, ::sort_size);
    spdlog::info("*** turning angle sum (degree) ***");
    spdlog::info("  min: [{}] {}", strand_tas_stats.min.idx, strand_tas_stats.min.turning_angle_sum);
    spdlog::info("  max: [{}] {}", strand_tas_stats.max.idx, strand_tas_stats.max.turning_angle_sum);
    spdlog::info("  median: [{}] {}", strand_tas_stats.median.idx, strand_tas_stats.median.turning_angle_sum);
    spdlog::info("  average: {}", strand_tas_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : strand_tas_stats.largest) spdlog::info("    [{}] {}", i.idx, i.turning_angle_sum);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : strand_tas_stats.smallest) spdlog::info("    [{}] {}", i.idx, i.turning_angle_sum);
    }

    spdlog::info("================================================================");
    spdlog::info("Segment stats:");
    spdlog::info("================================================================");
    const auto& segment_stats = util::get_stats(segment_info, [](const auto& a) { return a.length; }, ::sort_size);
    spdlog::info("*** length ***");
    spdlog::info("       [idx/strand_idx/local_idx]");
    spdlog::info("  min: [{}/{}/{}] {}", segment_stats.min.idx, segment_stats.min.strand_idx, segment_stats.min.local_idx, segment_stats.min.length);
    spdlog::info("  max: [{}/{}/{}] {}", segment_stats.max.idx, segment_stats.max.strand_idx, segment_stats.max.local_idx, segment_stats.max.length);
    spdlog::info("  median: [{}/{}/{}] {}", segment_stats.median.idx, segment_stats.median.strand_idx, segment_stats.median.local_idx, segment_stats.median.length);
    spdlog::info("  average: {}", segment_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : segment_stats.largest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.length);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : segment_stats.smallest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.length);
    }

    spdlog::info("================================================================");
    spdlog::info("Point stats:");
    spdlog::info("================================================================");
    const auto& point_crr_stats = util::get_stats(point_info, [](const auto& a) { return a.circumradius_reciprocal; }, ::sort_size);
    spdlog::info("*** circumradius reciprocal ***");
    spdlog::info("       [idx/strand_idx/local_idx]");
    spdlog::info("  min: [{}/{}/{}] {}", point_crr_stats.min.idx, point_crr_stats.min.strand_idx, point_crr_stats.min.local_idx, point_crr_stats.min.circumradius_reciprocal);
    spdlog::info("  max: [{}/{}/{}] {}", point_crr_stats.max.idx, point_crr_stats.max.strand_idx, point_crr_stats.max.local_idx, point_crr_stats.max.circumradius_reciprocal);
    spdlog::info("  median: [{}/{}/{}] {}", point_crr_stats.median.idx, point_crr_stats.median.strand_idx, point_crr_stats.median.local_idx, point_crr_stats.median.circumradius_reciprocal);
    spdlog::info("  average: {}", point_crr_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : point_crr_stats.largest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.circumradius_reciprocal);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : point_crr_stats.smallest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.circumradius_reciprocal);
    }
    spdlog::info("----------------------------------------------------------------");
    const auto& point_ta_stats = util::get_stats(point_info, [](const auto& a) { return a.turning_angle; }, ::sort_size);
    spdlog::info("*** turning angle (degree) ***");
    spdlog::info("       [idx/strand_idx/local_idx]");
    spdlog::info("  min: [{}/{}/{}] {}", point_ta_stats.min.idx, point_ta_stats.min.strand_idx, point_ta_stats.min.local_idx, point_ta_stats.min.turning_angle);
    spdlog::info("  max: [{}/{}/{}] {}", point_ta_stats.max.idx, point_ta_stats.max.strand_idx, point_ta_stats.max.local_idx, point_ta_stats.max.turning_angle);
    spdlog::info("  median: [{}/{}/{}] {}", point_ta_stats.median.idx, point_ta_stats.median.strand_idx, point_ta_stats.median.local_idx, point_ta_stats.median.turning_angle);
    spdlog::info("  average: {}", point_ta_stats.average);
    if (::sort_size > 0) {
        spdlog::info("  top {} largest:", ::sort_size);
        for (const auto& i : point_ta_stats.largest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.turning_angle);
        spdlog::info("  top {} smallest:", ::sort_size);
        for (const auto& i : point_ta_stats.smallest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, i.turning_angle);
    }

    return {};
}
