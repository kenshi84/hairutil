#include "io.h"

#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic;

namespace {
void load_abc_sub(int depth, const Abc::IObject& obj, const Abc::M44d& parent_transform, std::vector<float>& points, std::vector<unsigned short>& segments) {
    const std::string spaces(depth * 2, ' ');
    Abc::M44d current_transform = parent_transform;

    std::string type = "unknown";
    if (AbcGeom::IPolyMeshSchema::matches(obj.getMetaData())) type = "mesh";
    if (AbcGeom::ICameraSchema::matches(obj.getMetaData())) type = "camera";
    if (AbcGeom::IXformSchema::matches(obj.getMetaData())) type = "transform";
    if (AbcGeom::IPointsSchema::matches(obj.getMetaData())) type = "points";
    if (AbcGeom::ICurvesSchema::matches(obj.getMetaData())) type = "curves";
    if (AbcGeom::INuPatchSchema::matches(obj.getMetaData())) type = "nurbs";
    if (AbcGeom::ISubDSchema::matches(obj.getMetaData())) type = "subdiv";
    if (AbcGeom::ILightSchema::matches(obj.getMetaData())) type = "light";
    if (AbcGeom::IFaceSetSchema::matches(obj.getMetaData())) type = "faceset";

    log_info("{}{} ({})", spaces, obj.getName(), type);

    if (type == "transform") {
        AbcGeom::IXform xform(obj, Abc::kWrapExisting);
        AbcGeom::IXformSchema& schema = xform.getSchema();
        const size_t num_samples = schema.getNumSamples();

        if (num_samples > 1) {
            log_warn("Alembic transform \"{}\" has {} samples; using the first frame and ignoring frames 2 through {}.",
                     obj.getName(), num_samples, num_samples);
        }

        AbcGeom::IXformSchema::sample_type sample = schema.getValue(Abc::ISampleSelector(static_cast<Abc::index_t>(0)));
        const Abc::M44d local_transform = sample.getMatrix();
        current_transform = sample.getInheritsXforms() ? local_transform * parent_transform : local_transform;
    }

    if (type == "curves") {
        AbcGeom::ICurves curves(obj, Abc::kWrapExisting);
        AbcGeom::ICurvesSchema::sample_type sample;
        curves.getSchema().get(sample);

        const size_t num_curves = sample.getNumCurves();
        const size_t num_points = sample.getPositions()->size();

        log_info("{}  num curves: {}", spaces, num_curves);
        log_info("{}  num points: {}", spaces, num_points);

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
            const auto& p = sample.getPositions()->get()[i];
            const Abc::V3d point(p.x, p.y, p.z);
            Abc::V3d transformed_point;
            current_transform.multVecMatrix(point, transformed_point);
            new_points[3*i + 0] = static_cast<float>(transformed_point.x);
            new_points[3*i + 1] = static_cast<float>(transformed_point.y);
            new_points[3*i + 2] = static_cast<float>(transformed_point.z);
        }
        points.insert(points.end(), new_points.begin(), new_points.end());
    }

    for (size_t i = 0; i < obj.getNumChildren(); i++) {
        load_abc_sub(depth + 1, obj.getChild(i), current_transform, points, segments);
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
    Abc::M44d identity;
    identity.makeIdentity();
    load_abc_sub(0, archive.getTop(), identity, points, segments);

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
    std::vector<std::int32_t> nVertices(header.hair_count);
    for (unsigned int i = 0; i < header.hair_count; ++i) {
        const unsigned short nsegs = (header.arrays & _CY_HAIR_FILE_SEGMENTS_BIT) ? hairfile->GetSegmentsArray()[i] : header.d_segments;
        nVertices[i] = static_cast<std::int32_t>(nsegs) + 1;
    }

    // Write to file
    Abc::OArchive archive(AbcCoreOgawa::WriteArchive(), filename);
    AbcGeom::OCurves curves(archive.getTop(), "curves");
    AbcGeom::OCurvesSchema::Sample sample;
    sample.setPositions(positions);
    sample.setCurvesNumVertices(nVertices);
    sample.setType(AbcGeom::kLinear);
    sample.setWrap(AbcGeom::kNonPeriodic);
    sample.setBasis(AbcGeom::kNoBasis);
    curves.getSchema().set(sample);
}
