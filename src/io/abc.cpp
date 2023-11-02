#include "io.h"

#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic;

namespace {
void load_abc_sub(int depth, const Abc::IObject& obj, std::vector<float>& points, std::vector<unsigned short>& segments) {
    const std::string spaces(depth * 2, ' ');

    std::string type = "unknown";
    const std::string schema = "schema";
    if (AbcGeom::IPolyMeshSchema::matches(obj.getMetaData())) type = "mesh";
    if (AbcGeom::ICameraSchema::matches(obj.getMetaData())) type = "camera";
    if (AbcGeom::IXformSchema::matches(obj.getMetaData())) type = "transform";
    if (AbcGeom::IPointsSchema::matches(obj.getMetaData())) type = "points";
    if (AbcGeom::ICurvesSchema::matches(obj.getMetaData())) type = "curves";
    if (AbcGeom::INuPatchSchema::matches(obj.getMetaData())) type = "nurbs";
    if (AbcGeom::ISubDSchema::matches(obj.getMetaData())) type = "subdiv";
    if (AbcGeom::ILightSchema::matches(obj.getMetaData())) type = "light";
    if (AbcGeom::IFaceSetSchema::matches(obj.getMetaData())) type = "faceset";

    spdlog::info("{}{} ({})", spaces, obj.getName(), type);

    if (type == "transform") {
        AbcGeom::IXformSchema xform(obj, Abc::kWrapExisting);
        AbcGeom::IXformSchema::sample_type sample;
        xform.get(sample);

        if (sample.getNumOps() > 0) {
            throw std::runtime_error("Transform is unsupported!");
        }
    }

    if (type == "curves") {
        AbcGeom::ICurves curves(obj, Abc::kWrapExisting);
        AbcGeom::ICurvesSchema::sample_type sample;
        curves.getSchema().get(sample);

        const size_t num_curves = sample.getNumCurves();
        const size_t num_points = sample.getPositions()->size();

        spdlog::info("{}  num curves: {}", spaces, num_curves);
        spdlog::info("{}  num points: {}", spaces, num_points);

        // Copy segments data
        segments.reserve(segments.size() + num_curves);
        int num_points_check = 0;
        for (size_t i = 0; i < num_curves; ++i) {
            segments.push_back((*sample.getCurvesNumVertices())[i] - 1);
            num_points_check += (*sample.getCurvesNumVertices())[i];
        }

        if (num_points_check != num_points) {
            throw std::runtime_error(fmt::format("Sum of curvesNumVertices does not match number of points! {} vs {}", num_points_check, num_points));
        }

        // Copy points data
        std::vector<float> new_points(num_points * 3);
        for (size_t i = 0; i < num_points; ++i) {
            std::memcpy(new_points.data() + 3*i, &sample.getPositions()->get()[i].x, 3*sizeof(float));
        }
        points.insert(points.end(), new_points.begin(), new_points.end());
    }

    for (size_t i = 0; i < obj.getNumChildren(); i++) {
        load_abc_sub(depth + 1, obj.getChild(i), points, segments);
    }
}
}

std::shared_ptr<cyHairFile> io::load_abc(const std::string &filename) {
    AbcCoreFactory::IFactory factory;
    Abc::IArchive archive = factory.getArchive(filename);

    if (!archive.valid()) {
        throw std::runtime_error(fmt::format("Failed to open Alembic file \"{}\"", filename));
    }

    std::vector<float> points;
    std::vector<unsigned short> segments;

    // Read data recursively
    load_abc_sub(0, archive.getTop(), points, segments);

    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(_CY_HAIR_FILE_POINTS_BIT | _CY_HAIR_FILE_SEGMENTS_BIT);
    hairfile->SetHairCount(segments.size());
    hairfile->SetPointCount(points.size() / 3);

    std::memcpy(hairfile->GetPointsArray(), points.data(), points.size() * sizeof(float));
    std::memcpy(hairfile->GetSegmentsArray(), segments.data(), segments.size() * sizeof(unsigned short));

    return hairfile;
}

void io::save_abc(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    const auto& header = hairfile->GetHeader();

    // Fill positions data
    std::vector<Abc::V3f> positions(header.point_count);
    for (unsigned int i = 0; i < header.point_count; ++i) {
        std::memcpy(&positions[i].x, &hairfile->GetPointsArray()[i * 3], 3*sizeof(float));
    }

    // Fill nVertices data
    std::vector<int> nVertices(header.hair_count);
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned short nsegs = header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT ? hairfile->GetSegmentsArray()[i] : header.d_segments;
        nVertices[i] = nsegs + 1;
    }

    // Write to file
    Abc::OArchive archive(AbcCoreOgawa::WriteArchive(), filename);
    AbcGeom::OCurves curves(archive.getTop(), "curves");
    AbcGeom::OCurvesSchema::Sample sample(positions, nVertices);
    curves.getSchema().set(sample);
}
