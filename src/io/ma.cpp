#include "io.h"

using namespace Eigen;

std::shared_ptr<cyHairFile> io::load_ma(const std::string &filename) {
    std::ifstream ifs;
    ifs.open(filename.c_str());

    // Skip the first two lines
    std::string line;
    std::getline(ifs, line);
    std::getline(ifs, line);

    // Read data into std::vector
    std::vector<float> points_array;
    std::vector<unsigned short> segments_array;
    points_array.reserve(200000 * 3);
    segments_array.reserve(200000);

    // Loop until the end of file
    for ( ; std::getline(ifs, line); ) {
        if (segments_array.size() && segments_array.size() % 100 == 0)
            spdlog::debug("Processing hair {}", segments_array.size());

        // Skip the first 5 lines
        for (int i = 0; i < 5; ++i)
            std::getline(ifs, line);

        // Read the number of vertices
        std::getline(ifs, line);
        std::istringstream iss(line);
        unsigned int num_vertices;
        iss >> num_vertices;

        segments_array.push_back(num_vertices - 1);

        // Read individual vertices
        for (int i = 0; i < num_vertices; ++i) {
            std::getline(ifs, line);
            std::istringstream iss(line);
            float x, y, z;
            iss >> x >> y >> z;
            points_array.push_back(x);
            points_array.push_back(y);
            points_array.push_back(z);
        }

        // Skip the last line
        getline(ifs, line);
    }

    // Create cyHairFile
    auto hairfile = std::make_shared<cyHairFile>();
    hairfile->SetArrays(_CY_HAIR_FILE_SEGMENTS_BIT | _CY_HAIR_FILE_POINTS_BIT);
    hairfile->SetHairCount(segments_array.size());
    hairfile->SetPointCount(points_array.size() / 3);

    // Copy content of segments_array and points_array to hairfile
    std::memcpy(hairfile->GetSegmentsArray(), segments_array.data(), segments_array.size() * sizeof(unsigned short));
    std::memcpy(hairfile->GetPointsArray(), points_array.data(), points_array.size() * sizeof(float));

    return hairfile;
}

void io::save_ma(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    std::ofstream ofs(filename.c_str());

    ofs << "requires maya \"2014\";" << endl;
    ofs << "createNode transform -n \"group1\";" << endl;

    int point_idx = 0;

    const auto& header = hairfile->GetHeader();

    for (int hair_idx = 0; hair_idx < header.hair_count; ++hair_idx) {
        if (hair_idx && (hair_idx % 100 == 0))
            spdlog::debug("Processing hair {}/{}", hair_idx, header.hair_count);

        ofs << "createNode transform -n \"curve" << (hair_idx + 1) << "\" -p \"group1\";" << endl;
        ofs << "createNode nurbsCurve -n \"curveShape" << (hair_idx + 1) << "\" -p \"curve" << (hair_idx + 1) << "\";" << endl;
        ofs << "    setAttr -k off \".v\";" << endl;
        ofs << "    setAttr \".cc\" -type \"nurbsCurve\"" << endl;

        int s = hairfile->GetSegmentsArray() ? hairfile->GetSegmentsArray()[hair_idx] : header.d_segments;

        ofs << "        1 " << s << " 0 no 3" << endl;

        ofs << "        " << (s + 1);
        for (int i = 0; i <= s; ++i)
            ofs << " " << i;
        ofs << endl;

        ofs << "        " << (s + 1) << endl;

        for (int i = 0; i <= s; ++i, ++point_idx)
            ofs << "        " << Map<RowVector3f>(&hairfile->GetPointsArray()[3 * point_idx]) << endl;

        ofs << "        ;" << endl;
    }
}
