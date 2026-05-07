#include "cmd.h"
#include "util.h"

#include <happly.h>

void cmd::parse::getroots(args::Subparser &parser) {
    parser.Parse();
    globals::cmd_exec = cmd::exec::getroots;
    globals::check_error = []() {
        const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_roots.ply", globals::output_dir);
        if (!globals::overwrite && std::filesystem::exists(output_file)) {
            throw std::runtime_error("File already exists: " + output_file + ". Use --overwrite to overwrite.");
        }
    };
}

std::shared_ptr<cyHairFile> cmd::exec::getroots(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header = hairfile_in->GetHeader();

    std::vector<float> vertex_x;    vertex_x.reserve(header.hair_count);
    std::vector<float> vertex_y;    vertex_y.reserve(header.hair_count);
    std::vector<float> vertex_z;    vertex_z.reserve(header.hair_count);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned short nsegs = (header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) ? hairfile_in->GetSegmentsArray()[i] : header.d_segments;
        vertex_x.push_back(hairfile_in->GetPointsArray()[3 * offset + 0]);
        vertex_y.push_back(hairfile_in->GetPointsArray()[3 * offset + 1]);
        vertex_z.push_back(hairfile_in->GetPointsArray()[3 * offset + 2]);
        offset += nsegs + 1;
    }

    happly::PLYData ply;
    ply.addElement("vertex", header.hair_count);
    ply.getElement("vertex").addProperty<float>("x", vertex_x);
    ply.getElement("vertex").addProperty<float>("y", vertex_y);
    ply.getElement("vertex").addProperty<float>("z", vertex_z);

    const std::string output_file = util::path_under_optional_dir(globals::input_file_wo_ext + "_roots.ply", globals::output_dir);
    ply.write(output_file, globals::ply_save_ascii ? happly::DataFormat::ASCII : happly::DataFormat::Binary);
    log_info("Written to {}", output_file);

    return {};
}
