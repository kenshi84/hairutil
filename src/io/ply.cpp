#include "io.h"

#include <happly.h>

std::shared_ptr<cyHairFile> io::load_ply(const std::string &filename) {
    happly::PLYData ply(filename);

    // Error if it doesn't have "vertex" element
    if (!ply.hasElement("vertex")) {
        throw std::runtime_error("PLY file does not have \"vertex\" element");
    }
    auto& vertex = ply.getElement("vertex");

    // Error if it doesn't have "x", "y", "z" properties
    if (!vertex.hasProperty("x") || !vertex.hasProperty("y") || !vertex.hasProperty("z")) {
        throw std::runtime_error("PLY file does not have \"x\", \"y\", \"z\" properties");
    }
    const std::vector<float> vertex_x = vertex.getProperty<float>("x");
    const std::vector<float> vertex_y = vertex.getProperty<float>("y");
    const std::vector<float> vertex_z = vertex.getProperty<float>("z");

    // Read color if available
    std::vector<unsigned char> vertex_red;
    std::vector<unsigned char> vertex_green;
    std::vector<unsigned char> vertex_blue;
    if (vertex.hasProperty("red") && vertex.hasProperty("green") && vertex.hasProperty("blue")) {
        spdlog::debug("PLY file has \"red\", \"green\", \"blue\" properties");
        vertex_red = vertex.getProperty<unsigned char>("red");
        vertex_green = vertex.getProperty<unsigned char>("green");
        vertex_blue = vertex.getProperty<unsigned char>("blue");
    }

    // Read transparency if available
    std::vector<unsigned char> vertex_alpha;
    if (vertex.hasProperty("alpha")) {
        spdlog::debug("PLY file has \"alpha\" property");
        vertex_alpha = vertex.getProperty<unsigned char>("alpha");
    }

    // Read thickness if available
    std::vector<float> vertex_thickness;
    if (vertex.hasProperty("thickness")) {
        spdlog::debug("PLY file has \"thickness\" property");
        vertex_thickness = vertex.getProperty<float>("thickness");
    }

    // Read "strand" info if available
    std::vector<unsigned short> segments_array;
    if (ply.hasElement("strand")) {
        auto& strand = ply.getElement("strand");
        if (strand.hasProperty("nsegs")) {
            spdlog::debug("PLY file has \"strand\" element with \"nsegs\" property");
            segments_array = strand.getProperty<unsigned short>("nsegs");
        }
    }

    // Error checking when segments_array is empty
    if (segments_array.empty()) {
        // Error if globals::ply_load_default_nsegs is not set
        if (globals::ply_load_default_nsegs == 0) {
            throw std::runtime_error("PLY file does not have \"strand\" element with \"nsegs\" property, and --ply-load-default-nsegs is not set");
        }

        // Error if globals::ply_load_default_nsegs is not a divisor of the number of vertices
        if (vertex_x.size() % (globals::ply_load_default_nsegs + 1) != 0) {
            throw std::runtime_error("PLY file does not have \"strand\" element with \"nsegs\" property, and --ply-load-default-nsegs + 1 is not a divisor of the number of vertices");
        }
    }

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(
        _CY_HAIR_FILE_POINTS_BIT |
        (segments_array.empty() ? 0 : _CY_HAIR_FILE_SEGMENTS_BIT) |
        (vertex_red.empty() ? 0 : _CY_HAIR_FILE_COLORS_BIT) |
        (vertex_alpha.empty() ? 0 : _CY_HAIR_FILE_TRANSPARENCY_BIT) |
        (vertex_thickness.empty() ? 0 : _CY_HAIR_FILE_THICKNESS_BIT)
    );

    hairfile->SetHairCount(segments_array.empty() ? (vertex_x.size() / (globals::ply_load_default_nsegs + 1)) : segments_array.size());
    hairfile->SetPointCount(vertex_x.size());

    for (size_t i = 0; i < vertex_x.size(); ++i) {
        // Copy to points array
        hairfile->GetPointsArray()[i * 3 + 0] = vertex_x[i];
        hairfile->GetPointsArray()[i * 3 + 1] = vertex_y[i];
        hairfile->GetPointsArray()[i * 3 + 2] = vertex_z[i];

        // Copy to colors array
        if (!vertex_red.empty()) {
            hairfile->GetColorsArray()[i * 3 + 0] = vertex_red[i] / 255.0f;
            hairfile->GetColorsArray()[i * 3 + 1] = vertex_green[i] / 255.0f;
            hairfile->GetColorsArray()[i * 3 + 2] = vertex_blue[i] / 255.0f;
        }

        // Copy to transparency array
        if (!vertex_alpha.empty()) {
            hairfile->GetTransparencyArray()[i] = vertex_alpha[i] / 255.0f;
        }

        // Copy to thickness array
        if (!vertex_thickness.empty()) {
            hairfile->GetThicknessArray()[i] = vertex_thickness[i];
        }
    }

    // Copy segments array if available, otherwise set default
    if (!segments_array.empty()) {
        std::copy(segments_array.begin(), segments_array.end(), hairfile->GetSegmentsArray());
    } else {
        hairfile->SetDefaultSegmentCount(globals::ply_load_default_nsegs);
    }

    return hairfile;
}

void io::save_ply(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    const auto& header = hairfile->GetHeader();

    // Create arrays for "vertex" element
    std::vector<float> vertex_x;                vertex_x.reserve(header.point_count);
    std::vector<float> vertex_y;                vertex_y.reserve(header.point_count);
    std::vector<float> vertex_z;                vertex_z.reserve(header.point_count);
    std::vector<unsigned char> vertex_red;      vertex_red.reserve(header.point_count);
    std::vector<unsigned char> vertex_green;    vertex_green.reserve(header.point_count);
    std::vector<unsigned char> vertex_blue;     vertex_blue.reserve(header.point_count);
    std::vector<unsigned char> vertex_alpha;    vertex_alpha.reserve(header.point_count);
    std::vector<float> vertex_thickness;        vertex_thickness.reserve(header.point_count);

    for (unsigned int i = 0; i < header.point_count; ++i) {
        vertex_x.push_back(hairfile->GetPointsArray()[i * 3 + 0]);
        vertex_y.push_back(hairfile->GetPointsArray()[i * 3 + 1]);
        vertex_z.push_back(hairfile->GetPointsArray()[i * 3 + 2]);

        if (header.arrays & _CY_HAIR_FILE_COLORS_BIT) {
            vertex_red.push_back(hairfile->GetColorsArray()[i * 3 + 0] * 255);
            vertex_green.push_back(hairfile->GetColorsArray()[i * 3 + 1] * 255);
            vertex_blue.push_back(hairfile->GetColorsArray()[i * 3 + 2] * 255);
        }

        if (header.arrays & _CY_HAIR_FILE_TRANSPARENCY_BIT) {
            vertex_alpha.push_back(hairfile->GetTransparencyArray()[i] * 255);
        }

        if (header.arrays & _CY_HAIR_FILE_THICKNESS_BIT) {
            vertex_thickness.push_back(hairfile->GetThicknessArray()[i]);
        }
    }

    // If color is not available, assign random value per strand
    if (vertex_red.empty()) {
        std::uniform_int_distribution<unsigned char> uniform_dist(0, 255);
        for (unsigned int i = 0; i < header.hair_count; ++i) {
            const unsigned short nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile->GetSegmentsArray()[i] : header.d_segments;

            const unsigned char r = uniform_dist(globals::rng);
            const unsigned char g = uniform_dist(globals::rng);
            const unsigned char b = uniform_dist(globals::rng);

            vertex_red.resize(vertex_red.size() + nsegs + 1, r);
            vertex_green.resize(vertex_green.size() + nsegs + 1, g);
            vertex_blue.resize(vertex_blue.size() + nsegs + 1, b);
        }
    }

    // Create array for "strand" element
    std::vector<unsigned short> segments_array;
    if (header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) {
        // Copy segments array if available
        std::copy(hairfile->GetSegmentsArray(), hairfile->GetSegmentsArray() + header.hair_count, std::back_inserter(segments_array));
    } else {
        // Otherwise, fill with default
        segments_array.resize(header.hair_count, header.d_segments);
    }

    happly::PLYData ply;

    // Add "vertex" element, add properties
    ply.addElement("vertex", header.point_count);
    auto& vertex = ply.getElement("vertex");

    vertex.addProperty<float>("x", vertex_x);
    vertex.addProperty<float>("y", vertex_y);
    vertex.addProperty<float>("z", vertex_z);

    if (!vertex_red.empty()) {
        vertex.addProperty<unsigned char>("red", vertex_red);
        vertex.addProperty<unsigned char>("green", vertex_green);
        vertex.addProperty<unsigned char>("blue", vertex_blue);
    }

    if (!vertex_alpha.empty()) {
        vertex.addProperty<unsigned char>("alpha", vertex_alpha);
    }

    if (!vertex_thickness.empty()) {
        vertex.addProperty<float>("thickness", vertex_thickness);
    }

    // Add "strand" element, add property
    ply.addElement("strand", header.hair_count);
    ply.getElement("strand").addProperty<unsigned short>("nsegs", segments_array);

    std::vector<int> edge_vertex1;      edge_vertex1.reserve(vertex_x.size());
    std::vector<int> edge_vertex2;      edge_vertex2.reserve(vertex_x.size());

    int point_idx = 0;
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        for (unsigned int j = 0; j < segments_array[i]; ++j) {
            edge_vertex1.push_back(point_idx);
            edge_vertex2.push_back(point_idx + 1);
            ++point_idx;
        }
        ++point_idx;
    }

    ply.addElement("edge", std::accumulate(segments_array.begin(), segments_array.end(), 0));
    ply.getElement("edge").addProperty<int>("vertex1", edge_vertex1);
    ply.getElement("edge").addProperty<int>("vertex2", edge_vertex2);

    // Write to file
    ply.write(filename, globals::ply_save_binary ? happly::DataFormat::Binary : happly::DataFormat::ASCII);
}
