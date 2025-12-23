#include "cmd.h"
#include "util.h"

#include <xlnt/xlnt.hpp>

using namespace Eigen;

namespace {

struct {
    unsigned int& sort_size = cmd::param::ui("stats", "sort_size");
    bool& no_export = cmd::param::b("stats", "no_export");
    bool& export_raw_strand = cmd::param::b("stats", "export_raw_strand");
    bool& export_raw_segment = cmd::param::b("stats", "export_raw_segment");
    bool& export_raw_point = cmd::param::b("stats", "export_raw_point");
    bool& no_print = cmd::param::b("stats", "no_print");
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
    float max_point_curvature = 0;
    float min_point_curvature = std::numeric_limits<float>::max();
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
    float curvature = 0;                    // Discrete curvature (turning_angle_rad / segment_length_average)
};

}

void cmd::parse::stats(args::Subparser &parser) {
    args::ValueFlag<unsigned int> sort_size(parser, "N", "Print top-N sorted list of items [10]", {"sort-size"}, 10);
    args::Flag no_export(parser, "no-export", "Do not export result to a .xlsx file", {"no-export"});
    args::Flag export_raw_strand(parser, "export-raw-strand", "Include raw strand data in exported file", {"export-raw-strand"});
    args::Flag export_raw_segment(parser, "export-raw-segment", "Include raw segment data in exported file", {"export-raw-segment"});
    args::Flag export_raw_point(parser, "export-raw-point", "Include raw point data in exported file", {"export-raw-point"});
    args::Flag no_print(parser, "no-print", "Do not print the stats", {"no-print"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::stats;
    globals::check_error = [](){
        if (::param.no_export && ::param.no_print) {
            throw std::runtime_error("Both --no-export and --no-print are specified");
        }
        if (::param.no_export && (::param.export_raw_strand || ::param.export_raw_segment || ::param.export_raw_point)) {
            throw std::runtime_error("Both --no-export and --export-raw-* are specified");
        }
        const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_stats.xlsx", globals::output_dir);
        if (!::param.no_export && !globals::overwrite && std::filesystem::exists(output_file)) {
            throw std::runtime_error("File already exists: " + output_file + ". Use --overwrite to overwrite.");
        }
    };
    ::param.sort_size = *sort_size;
    ::param.no_export = no_export;
    ::param.export_raw_strand = export_raw_strand;
    ::param.export_raw_segment = export_raw_segment;
    ::param.export_raw_point = export_raw_point;
    ::param.no_print = no_print;
}

std::shared_ptr<cyHairFile> cmd::exec::stats(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header = hairfile_in->GetHeader();

    std::vector<StrandInfo> strand_info_vec;    strand_info_vec.reserve(header.hair_count);
    std::vector<SegmentInfo> segment_info_vec;  segment_info_vec.reserve(header.point_count);
    std::vector<PointInfo> point_info_vec;      point_info_vec.reserve(header.point_count);

    // Collect raw data
    spdlog::info("Collecting raw data");
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
                const float turning_angle_rad = globals::pi - std::acos(std::clamp((la * la + lb * lb - lc * lc) / (2.0f * la * lb), -1.0f, 1.0f));
                const float turning_angle = turning_angle_rad * 180.0f / globals::pi;
                const float curvature = turning_angle_rad / ((la + lb) / 2.0f);

                PointInfo point_info;
                point_info.idx = offset + j + 1;
                point_info.strand_idx = i;
                point_info.local_idx = j + 1;
                point_info.circumradius_reciprocal = circumradius_reciprocal;
                point_info.turning_angle = turning_angle;
                point_info.curvature = curvature;
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
                strand_info.max_point_curvature = std::max(strand_info.max_point_curvature, curvature);
                strand_info.min_point_curvature = std::min(strand_info.min_point_curvature, curvature);
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

    xlnt::workbook wb;

    xlnt::worksheet ws_strand_stats = wb.active_sheet();
    xlnt::worksheet ws_segment_stats = wb.create_sheet();
    xlnt::worksheet ws_point_stats = wb.create_sheet();

    ws_strand_stats.title("Strand stats");
    ws_segment_stats.title("Segment stats");
    ws_point_stats.title("Point stats");

    const size_t max_num_rows = 1000000;    // Excel's limit

    if (::param.export_raw_strand) {
        // Strand raw data
        spdlog::info("Writing strand raw data");
        std::vector<xlnt::worksheet> ws_strand_raw(header.hair_count / max_num_rows + 1);
        for (size_t i = 0; i < ws_strand_raw.size(); ++i) {
            ws_strand_raw[i] = wb.create_sheet();
            ws_strand_raw[i].title(ws_strand_raw.size() > 1 ? fmt::format("Strand raw {}", i + 1) : "Strand raw");
        }
        for (size_t i = 0; i < ws_strand_raw.size(); ++i) {
            ws_strand_raw[i].cell("A1").value("idx");
            ws_strand_raw[i].cell("B1").value("nsegs");
            ws_strand_raw[i].cell("C1").value("length");
            ws_strand_raw[i].cell("D1").value("turning_angle_sum");
            ws_strand_raw[i].cell("E1").value("max_segment_length");
            ws_strand_raw[i].cell("F1").value("min_segment_length");
            ws_strand_raw[i].cell("G1").value("max_segment_turning_angle_diff");
            ws_strand_raw[i].cell("H1").value("min_segment_turning_angle_diff");
            ws_strand_raw[i].cell("I1").value("max_point_circumradius_reciprocal");
            ws_strand_raw[i].cell("J1").value("min_point_circumradius_reciprocal");
            ws_strand_raw[i].cell("K1").value("max_point_turning_angle");
            ws_strand_raw[i].cell("L1").value("min_point_turning_angle");
            ws_strand_raw[i].cell("M1").value("max_point_curvature");
            ws_strand_raw[i].cell("N1").value("min_point_curvature");
        }
        for (size_t i = 0; i < header.hair_count; ++i) {
            const size_t ws_idx = i / max_num_rows;
            xlnt::worksheet& ws = ws_strand_raw[ws_idx];
            ws.cell(fmt::format("A{}", i + 2 - ws_idx * max_num_rows)).value((int)strand_info_vec[i].idx);
            ws.cell(fmt::format("B{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].nsegs);
            ws.cell(fmt::format("C{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].length);
            ws.cell(fmt::format("D{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].turning_angle_sum);
            ws.cell(fmt::format("E{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].max_segment_length);
            ws.cell(fmt::format("F{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].min_segment_length);
            ws.cell(fmt::format("G{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].max_segment_turning_angle_diff);
            ws.cell(fmt::format("H{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].min_segment_turning_angle_diff);
            ws.cell(fmt::format("I{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].max_point_circumradius_reciprocal);
            ws.cell(fmt::format("J{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].min_point_circumradius_reciprocal);
            ws.cell(fmt::format("K{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].max_point_turning_angle);
            ws.cell(fmt::format("L{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].min_point_turning_angle);
            ws.cell(fmt::format("M{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].max_point_curvature);
            ws.cell(fmt::format("N{}", i + 2 - ws_idx * max_num_rows)).value(strand_info_vec[i].min_point_curvature);
        }
    }

    if (::param.export_raw_segment) {
        // Segment raw data
        spdlog::info("Writing segment raw data");
        std::vector<xlnt::worksheet> ws_segment_raw(segment_info_vec.size() / max_num_rows + 1);
        for (size_t i = 0; i < ws_segment_raw.size(); ++i) {
            ws_segment_raw[i] = wb.create_sheet();
            ws_segment_raw[i].title(ws_segment_raw.size() > 1 ? fmt::format("Segment raw {}", i + 1) : "Segment raw");
        }
        for (size_t i = 0; i < ws_segment_raw.size(); ++i) {
            ws_segment_raw[i].cell("A1").value("idx");
            ws_segment_raw[i].cell("B1").value("strand_idx");
            ws_segment_raw[i].cell("C1").value("local_idx");
            ws_segment_raw[i].cell("D1").value("length");
            ws_segment_raw[i].cell("E1").value("turning_angle_diff");
        }
        for (size_t i = 0; i < segment_info_vec.size(); ++i) {
            const size_t ws_idx = i / max_num_rows;
            xlnt::worksheet& ws = ws_segment_raw[ws_idx];
            ws.cell(fmt::format("A{}", i + 2 - ws_idx * max_num_rows)).value((int)segment_info_vec[i].idx);
            ws.cell(fmt::format("B{}", i + 2 - ws_idx * max_num_rows)).value(segment_info_vec[i].strand_idx);
            ws.cell(fmt::format("C{}", i + 2 - ws_idx * max_num_rows)).value(segment_info_vec[i].local_idx);
            ws.cell(fmt::format("D{}", i + 2 - ws_idx * max_num_rows)).value(segment_info_vec[i].length);
            ws.cell(fmt::format("E{}", i + 2 - ws_idx * max_num_rows)).value(segment_info_vec[i].turning_angle_diff);
        }
    }

    if (::param.export_raw_point) {
        // Point raw data
        spdlog::info("Writing point raw data");
        std::vector<xlnt::worksheet> ws_point_raw(point_info_vec.size() / max_num_rows + 1);
        for (size_t i = 0; i < ws_point_raw.size(); ++i) {
            ws_point_raw[i] = wb.create_sheet();
            ws_point_raw[i].title(ws_point_raw.size() > 1 ? fmt::format("Point raw {}", i + 1) : "Point raw");
        }
        for (size_t i = 0; i < ws_point_raw.size(); ++i) {
            ws_point_raw[i].cell("A1").value("idx");
            ws_point_raw[i].cell("B1").value("strand_idx");
            ws_point_raw[i].cell("C1").value("local_idx");
            ws_point_raw[i].cell("D1").value("circumradius_reciprocal");
            ws_point_raw[i].cell("E1").value("turning_angle");
            ws_point_raw[i].cell("F1").value("curvature");
        }
        for (size_t i = 0; i < point_info_vec.size(); ++i) {
            const size_t ws_idx = i / max_num_rows;
            xlnt::worksheet& ws = ws_point_raw[ws_idx];
            ws.cell(fmt::format("A{}", i + 2 - ws_idx * max_num_rows)).value((int)point_info_vec[i].idx);
            ws.cell(fmt::format("B{}", i + 2 - ws_idx * max_num_rows)).value(point_info_vec[i].strand_idx);
            ws.cell(fmt::format("C{}", i + 2 - ws_idx * max_num_rows)).value(point_info_vec[i].local_idx);
            ws.cell(fmt::format("D{}", i + 2 - ws_idx * max_num_rows)).value(point_info_vec[i].circumradius_reciprocal);
            ws.cell(fmt::format("E{}", i + 2 - ws_idx * max_num_rows)).value(point_info_vec[i].turning_angle);
            ws.cell(fmt::format("F{}", i + 2 - ws_idx * max_num_rows)).value(point_info_vec[i].curvature);
        }
    }

    // Compute stats
    spdlog::info("Computing stats");

    std::map<std::string, util::StatsInfo<StrandInfo>> strand_stats;
    strand_stats["length"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.length; }, ::param.sort_size);
    strand_stats["nsegs"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.nsegs; }, ::param.sort_size);
    strand_stats["turning_angle_sum"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.turning_angle_sum; }, ::param.sort_size);
    strand_stats["max_segment_length"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.max_segment_length; }, ::param.sort_size);
    strand_stats["min_segment_length"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.min_segment_length; }, ::param.sort_size);
    strand_stats["max_segment_turning_angle_diff"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.max_segment_turning_angle_diff; }, ::param.sort_size);
    strand_stats["min_segment_turning_angle_diff"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.min_segment_turning_angle_diff; }, ::param.sort_size);
    strand_stats["max_point_circumradius_reciprocal"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.max_point_circumradius_reciprocal; }, ::param.sort_size);
    strand_stats["min_point_circumradius_reciprocal"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.min_point_circumradius_reciprocal; }, ::param.sort_size);
    strand_stats["max_point_turning_angle"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.max_point_turning_angle; }, ::param.sort_size);
    strand_stats["min_point_turning_angle"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.min_point_turning_angle; }, ::param.sort_size);
    strand_stats["max_point_curvature"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.max_point_curvature; }, ::param.sort_size);
    strand_stats["min_point_curvature"] = util::get_stats(strand_info_vec, [](const auto& a) { return a.min_point_curvature; }, ::param.sort_size);

    std::map<std::string, util::StatsInfo<SegmentInfo>> segment_stats;
    segment_stats["length"] = util::get_stats(segment_info_vec, [](const auto& a) { return a.length; }, ::param.sort_size);
    segment_stats["turning_angle_diff"] = util::get_stats(segment_info_vec, [](const auto& a) { return a.turning_angle_diff; }, ::param.sort_size);

    std::map<std::string, util::StatsInfo<PointInfo>> point_stats;
    point_stats["circumradius_reciprocal"] = util::get_stats(point_info_vec, [](const auto& a) { return a.circumradius_reciprocal; }, ::param.sort_size);
    point_stats["turning_angle"] = util::get_stats(point_info_vec, [](const auto& a) { return a.turning_angle; }, ::param.sort_size);
    point_stats["curvature"] = util::get_stats(point_info_vec, [](const auto& a) { return a.curvature; }, ::param.sort_size);

    if (!::param.no_export) {
        spdlog::info("Writing stats");

        const auto cellstr = [](const std::string& col, size_t row){ return col + std::to_string(row); };
        size_t row;

        auto append_strand_stats = [&](const std::string& name, auto getvalue){
            ws_strand_stats.cell(cellstr("A", row)).value(name);
            ws_strand_stats.cell(cellstr("A", row)).fill(xlnt::fill::solid(xlnt::color::yellow()));
            ++row;
                                                                        ws_strand_stats.cell(cellstr("B", row)).value("idx");                                   ws_strand_stats.cell(cellstr("C", row)).value("value");                             ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("min");       ws_strand_stats.cell(cellstr("B", row)).value((int)strand_stats[name].min.idx);     ws_strand_stats.cell(cellstr("C", row)).value(getvalue(strand_stats[name].min));        ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("max");       ws_strand_stats.cell(cellstr("B", row)).value((int)strand_stats[name].max.idx);     ws_strand_stats.cell(cellstr("C", row)).value(getvalue(strand_stats[name].max));        ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("median");    ws_strand_stats.cell(cellstr("B", row)).value((int)strand_stats[name].median.idx);  ws_strand_stats.cell(cellstr("C", row)).value(getvalue(strand_stats[name].median));     ++row;

            ws_strand_stats.cell(cellstr("A", row)).value("average");   ws_strand_stats.cell(cellstr("B", row)).value(strand_stats[name].average);      ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("stddev");    ws_strand_stats.cell(cellstr("B", row)).value(strand_stats[name].stddev);       ++row;

            ws_strand_stats.cell(cellstr("A", row)).value(fmt::format("top {} largest", strand_stats[name].largest.size()));        ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("idx");       ws_strand_stats.cell(cellstr("B", row)).value("value");     ++row;
            for (size_t i = 0; i < strand_stats[name].largest.size(); ++i) {
                ws_strand_stats.cell(cellstr("A", row)).value((int)strand_stats[name].largest[i].idx);      ws_strand_stats.cell(cellstr("B", row)).value(getvalue(strand_stats[name].largest[i]));     ++row;
            }
            ws_strand_stats.cell(cellstr("A", row)).value(fmt::format("top {} smallest", strand_stats[name].smallest.size()));      ++row;
            ws_strand_stats.cell(cellstr("A", row)).value("idx");       ws_strand_stats.cell(cellstr("B", row)).value("value");     ++row;
            for (size_t i = 0; i < strand_stats[name].smallest.size(); ++i) {
                ws_strand_stats.cell(cellstr("A", row)).value((int)strand_stats[name].smallest[i].idx);     ws_strand_stats.cell(cellstr("B", row)).value(getvalue(strand_stats[name].smallest[i]));    ++row;
            }
            ++row;
        };

        auto append_other_stats = [&](xlnt::worksheet& ws_stats, auto& stats, const std::string& name, auto getvalue) {
            ws_stats.cell(cellstr("A", row)).value(name);
            ws_stats.cell(cellstr("A", row)).fill(xlnt::fill::solid(xlnt::color::yellow()));
            ++row;
                                                            ws_stats.cell(cellstr("B", row)).value("idx");                      ws_stats.cell(cellstr("C", row)).value("strand_idx");                       ws_stats.cell(cellstr("D", row)).value("local_idx");                        ws_stats.cell(cellstr("E", row)).value("value");                    ++row;
            ws_stats.cell(cellstr("A", row)).value("min");  ws_stats.cell(cellstr("B", row)).value((int)stats[name].min.idx);   ws_stats.cell(cellstr("C", row)).value((int)stats[name].min.strand_idx);    ws_stats.cell(cellstr("D", row)).value((int)stats[name].min.local_idx);     ws_stats.cell(cellstr("E", row)).value(getvalue(stats[name].min));  ++row;
            ws_stats.cell(cellstr("A", row)).value("max");  ws_stats.cell(cellstr("B", row)).value((int)stats[name].max.idx);   ws_stats.cell(cellstr("C", row)).value((int)stats[name].max.strand_idx);    ws_stats.cell(cellstr("D", row)).value((int)stats[name].max.local_idx);     ws_stats.cell(cellstr("E", row)).value(getvalue(stats[name].max));  ++row;
            ws_stats.cell(cellstr("A", row)).value("median");   ws_stats.cell(cellstr("B", row)).value((int)stats[name].median.idx);    ws_stats.cell(cellstr("C", row)).value((int)stats[name].median.strand_idx); ws_stats.cell(cellstr("D", row)).value((int)stats[name].median.local_idx);  ws_stats.cell(cellstr("E", row)).value(getvalue(stats[name].median));   ++row;

            ws_stats.cell(cellstr("A", row)).value("average");  ws_stats.cell(cellstr("B", row)).value(stats[name].average);    ++row;
            ws_stats.cell(cellstr("A", row)).value("stddev");   ws_stats.cell(cellstr("B", row)).value(stats[name].stddev);     ++row;

            ws_stats.cell(cellstr("A", row)).value(fmt::format("top {} largest", stats[name].largest.size()));              ++row;
            ws_stats.cell(cellstr("A", row)).value("idx");      ws_stats.cell(cellstr("B", row)).value("strand_idx");       ws_stats.cell(cellstr("C", row)).value("local_idx");    ws_stats.cell(cellstr("D", row)).value("value");    ++row;
            for (size_t i = 0; i < stats[name].largest.size(); ++i) {
                ws_stats.cell(cellstr("A", row)).value((int)stats[name].largest[i].idx);       ws_stats.cell(cellstr("B", row)).value((int)stats[name].largest[i].strand_idx);    ws_stats.cell(cellstr("C", row)).value((int)stats[name].largest[i].local_idx);     ws_stats.cell(cellstr("D", row)).value(getvalue(stats[name].largest[i]));    ++row;
            }
            ws_stats.cell(cellstr("A", row)).value(fmt::format("top {} smallest", stats[name].smallest.size()));            ++row;
            ws_stats.cell(cellstr("A", row)).value("idx");      ws_stats.cell(cellstr("B", row)).value("strand_idx");       ws_stats.cell(cellstr("C", row)).value("local_idx");    ws_stats.cell(cellstr("D", row)).value("value");    ++row;
            for (size_t i = 0; i < stats[name].smallest.size(); ++i) {
                ws_stats.cell(cellstr("A", row)).value((int)stats[name].smallest[i].idx);      ws_stats.cell(cellstr("B", row)).value((int)stats[name].smallest[i].strand_idx);   ws_stats.cell(cellstr("C", row)).value((int)stats[name].smallest[i].local_idx);    ws_stats.cell(cellstr("D", row)).value(getvalue(stats[name].smallest[i]));   ++row;
            }
            ++row;
        };

        ws_strand_stats.cell("A1").value("#strands:");
        ws_strand_stats.cell("B1").value((int)header.hair_count);
        ws_strand_stats.cell("A2").value("#points:");
        ws_strand_stats.cell("B2").value((int)header.point_count);

        row = 4;
        append_strand_stats("length", [](const auto& a){ return a.length; });
        append_strand_stats("nsegs", [](const auto& a){ return a.nsegs; });
        append_strand_stats("turning_angle_sum", [](const auto& a){ return a.turning_angle_sum; });
        append_strand_stats("max_segment_length", [](const auto& a){ return a.max_segment_length; });
        append_strand_stats("min_segment_length", [](const auto& a){ return a.min_segment_length; });
        append_strand_stats("max_segment_turning_angle_diff", [](const auto& a){ return a.max_segment_turning_angle_diff; });
        append_strand_stats("min_segment_turning_angle_diff", [](const auto& a){ return a.min_segment_turning_angle_diff; });
        append_strand_stats("max_point_circumradius_reciprocal", [](const auto& a){ return a.max_point_circumradius_reciprocal; });
        append_strand_stats("min_point_circumradius_reciprocal", [](const auto& a){ return a.min_point_circumradius_reciprocal; });
        append_strand_stats("max_point_turning_angle", [](const auto& a){ return a.max_point_turning_angle; });
        append_strand_stats("min_point_turning_angle", [](const auto& a){ return a.min_point_turning_angle; });
        append_strand_stats("max_point_curvature", [](const auto& a){ return a.max_point_curvature; });
        append_strand_stats("min_point_curvature", [](const auto& a){ return a.min_point_curvature; });

        row = 1;
        append_other_stats(ws_segment_stats, segment_stats, "length", [](const auto& a){ return a.length; });
        append_other_stats(ws_segment_stats, segment_stats, "turning_angle_diff", [](const auto& a){ return a.turning_angle_diff; });

        row = 1;
        append_other_stats(ws_point_stats, point_stats, "circumradius_reciprocal", [](const auto& a){ return a.circumradius_reciprocal; });
        append_other_stats(ws_point_stats, point_stats, "turning_angle", [](const auto& a){ return a.turning_angle; });
        append_other_stats(ws_point_stats, point_stats, "curvature", [](const auto& a){ return a.curvature; });
    }

    if (!::param.no_print) {
        auto print_strand_stats = [&](const std::string& name, auto getvalue) {
            spdlog::info("----------------------------------------------------------------");
            spdlog::info("*** {} ***", name);
            spdlog::info("  min: [{}] {}", strand_stats[name].min.idx, getvalue(strand_stats[name].min));
            spdlog::info("  max: [{}] {}", strand_stats[name].max.idx, getvalue(strand_stats[name].max));
            spdlog::info("  median: [{}] {}", strand_stats[name].median.idx, getvalue(strand_stats[name].median));
            spdlog::info("  average (stddev): {} ({})", strand_stats[name].average, strand_stats[name].stddev);
            if (::param.sort_size > 0) {
                const size_t n = strand_stats[name].largest.size();
                spdlog::info("  top {} largest:", n);
                for (const auto& i : strand_stats[name].largest) spdlog::info("    [{}] {}", i.idx, getvalue(i));
                spdlog::info("  top {} smallest:", n);
                for (const auto& i : strand_stats[name].smallest) spdlog::info("    [{}] {}", i.idx, getvalue(i));
            }
        };

        auto print_other_stats = [&](auto& stats, const std::string& name, auto getvalue) {
            spdlog::info("----------------------------------------------------------------");
            spdlog::info("*** {} ***", name);
            spdlog::info("       [idx/strand_idx/local_idx]");
            spdlog::info("  min: [{}/{}/{}] {}", stats[name].min.idx, stats[name].min.strand_idx, stats[name].min.local_idx, getvalue(stats[name].min));
            spdlog::info("  max: [{}/{}/{}] {}", stats[name].max.idx, stats[name].max.strand_idx, stats[name].max.local_idx, getvalue(stats[name].max));
            spdlog::info("  median: [{}/{}/{}] {}", stats[name].median.idx, stats[name].median.strand_idx, stats[name].median.local_idx, getvalue(stats[name].median));
            spdlog::info("  average (stddev): {} ({})", stats[name].average, stats[name].stddev);
            if (::param.sort_size > 0) {
                const size_t n = stats[name].largest.size();
                spdlog::info("  top {} largest:", n);
                for (const auto& i : stats[name].largest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, getvalue(i));
                spdlog::info("  top {} smallest:", n);
                for (const auto& i : stats[name].smallest) spdlog::info("    [{}/{}/{}] {}", i.idx, i.strand_idx, i.local_idx, getvalue(i));
            }
        };

        spdlog::info("================================================================");
        spdlog::info("Strand stats:");
        print_strand_stats("length", [](const auto& a){ return a.length; });
        print_strand_stats("nsegs", [](const auto& a){ return a.nsegs; });
        print_strand_stats("turning_angle_sum", [](const auto& a){ return a.turning_angle_sum; });
        print_strand_stats("max_segment_length", [](const auto& a){ return a.max_segment_length; });
        print_strand_stats("min_segment_length", [](const auto& a){ return a.min_segment_length; });
        print_strand_stats("max_segment_turning_angle_diff", [](const auto& a){ return a.max_segment_turning_angle_diff; });
        print_strand_stats("min_segment_turning_angle_diff", [](const auto& a){ return a.min_segment_turning_angle_diff; });
        print_strand_stats("max_point_circumradius_reciprocal", [](const auto& a){ return a.max_point_circumradius_reciprocal; });
        print_strand_stats("min_point_circumradius_reciprocal", [](const auto& a){ return a.min_point_circumradius_reciprocal; });
        print_strand_stats("max_point_turning_angle", [](const auto& a){ return a.max_point_turning_angle; });
        print_strand_stats("min_point_turning_angle", [](const auto& a){ return a.min_point_turning_angle; });
        print_strand_stats("max_point_curvature", [](const auto& a){ return a.max_point_curvature; });
        print_strand_stats("min_point_curvature", [](const auto& a){ return a.min_point_curvature; });

        spdlog::info("================================================================");
        spdlog::info("Segment stats:");
        print_other_stats(segment_stats, "length", [](const auto& a){ return a.length; });
        print_other_stats(segment_stats, "turning_angle_diff", [](const auto& a){ return a.turning_angle_diff; });

        spdlog::info("================================================================");
        spdlog::info("Point stats:");
        print_other_stats(point_stats, "circumradius_reciprocal", [](const auto& a){ return a.circumradius_reciprocal; });
        print_other_stats(point_stats, "turning_angle", [](const auto& a){ return a.turning_angle; });
        print_other_stats(point_stats, "curvature", [](const auto& a){ return a.curvature; });
    }

    if (!::param.no_export) {
        const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_stats.xlsx", globals::output_dir);
        spdlog::info("Saving to {}", output_file);
        wb.save(output_file);
    }

    return {};
}
