#include "io.h"

#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#include <limits>

using namespace Alembic;

namespace {
std::string curve_type_name(const AbcGeom::CurveType type) {
    switch (type) {
        case AbcGeom::kCubic: return "cubic";
        case AbcGeom::kLinear: return "linear";
        case AbcGeom::kVariableOrder: return "variable-order";
        default: return fmt::format("unknown({})", static_cast<int>(type));
    }
}

std::string curve_wrap_name(const AbcGeom::CurvePeriodicity wrap) {
    switch (wrap) {
        case AbcGeom::kNonPeriodic: return "non-periodic";
        case AbcGeom::kPeriodic: return "periodic";
        default: return fmt::format("unknown({})", static_cast<int>(wrap));
    }
}

std::string curve_schema_desc(const AbcGeom::ICurvesSchema::sample_type& sample) {
    return fmt::format("type={}, basis={}, wrap={}",
                       curve_type_name(sample.getType()),
                       AbcGeom::GetBasisNameFromBasisType(sample.getBasis()),
                       curve_wrap_name(sample.getWrap()));
}

[[noreturn]] void throw_unsupported_curve_schema(const std::string& path, const AbcGeom::ICurvesSchema::sample_type& sample) {
    throw std::runtime_error(fmt::format("Unsupported Alembic curve schema at \"{}\": {}", path, curve_schema_desc(sample)));
}

void append_transformed_point(const Abc::V3d& point, const Abc::M44d& transform, std::vector<float>& points) {
    Abc::V3d transformed_point;
    transform.multVecMatrix(point, transformed_point);
    points.push_back(static_cast<float>(transformed_point.x));
    points.push_back(static_cast<float>(transformed_point.y));
    points.push_back(static_cast<float>(transformed_point.z));
}

Abc::V3d evaluate_cubic_bspline(const Abc::P3fArraySamplePtr& positions, const size_t offset, const size_t span, const double t) {
    const double omt = 1.0 - t;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double b0 = omt * omt * omt / 6.0;
    const double b1 = (4.0 - 6.0 * t2 + 3.0 * t3) / 6.0;
    const double b2 = (1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3) / 6.0;
    const double b3 = t3 / 6.0;

    const auto& p0 = positions->get()[offset + span + 0];
    const auto& p1 = positions->get()[offset + span + 1];
    const auto& p2 = positions->get()[offset + span + 2];
    const auto& p3 = positions->get()[offset + span + 3];

    return Abc::V3d(
        b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
        b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y,
        b0 * p0.z + b1 * p1.z + b2 * p2.z + b3 * p3.z
    );
}

size_t find_knot_span(const Abc::FloatArraySamplePtr& knots, const size_t knot_offset, const size_t num_vertices, const double u) {
    constexpr size_t degree = 3;
    const size_t n = num_vertices - 1;
    const float* const knot = knots->get() + knot_offset;

    if (u >= knot[n + 1]) return n;
    if (u <= knot[degree]) return degree;

    size_t low = degree;
    size_t high = n + 1;
    size_t mid = (low + high) / 2;
    while (u < knot[mid] || u >= knot[mid + 1]) {
        if (u < knot[mid]) {
            high = mid;
        } else {
            low = mid;
        }
        mid = (low + high) / 2;
    }
    return mid;
}

Abc::V3d evaluate_cubic_bspline_with_knots(const Abc::P3fArraySamplePtr& positions, const size_t offset, const size_t num_vertices, const Abc::FloatArraySamplePtr& knots, const size_t knot_offset, const double u) {
    constexpr size_t degree = 3;
    const size_t span = find_knot_span(knots, knot_offset, num_vertices, u);
    const float* const knot = knots->get() + knot_offset;
    std::array<Abc::V3d, degree + 1> d;

    for (size_t j = 0; j <= degree; ++j) {
        const auto& p = positions->get()[offset + span - degree + j];
        d[j] = Abc::V3d(p.x, p.y, p.z);
    }

    for (size_t r = 1; r <= degree; ++r) {
        for (size_t j = degree; j >= r; --j) {
            const double denom = knot[span + 1 + j - r] - knot[span - degree + j];
            const double alpha = denom == 0.0 ? 0.0 : (u - knot[span - degree + j]) / denom;
            d[j] = d[j - 1] * (1.0 - alpha) + d[j] * alpha;
            if (j == r) break;
        }
    }

    return d[degree];
}

void append_linear_curve(const Abc::P3fArraySamplePtr& positions, const size_t offset, const size_t num_vertices, const Abc::M44d& transform, std::vector<float>& points) {
    for (size_t i = 0; i < num_vertices; ++i) {
        const auto& p = positions->get()[offset + i];
        append_transformed_point(Abc::V3d(p.x, p.y, p.z), transform, points);
    }
}

size_t append_deduplicated_points(const std::vector<float>& curve_points, std::vector<float>& points) {
    bool has_previous = false;
    float prev_x = 0.0f;
    float prev_y = 0.0f;
    float prev_z = 0.0f;
    size_t count = 0;

    for (size_t i = 0; i < curve_points.size(); i += 3) {
        const float x = curve_points[i + 0];
        const float y = curve_points[i + 1];
        const float z = curve_points[i + 2];
        if (has_previous && prev_x == x && prev_y == y && prev_z == z) {
            continue;
        }

        points.push_back(x);
        points.push_back(y);
        points.push_back(z);
        prev_x = x;
        prev_y = y;
        prev_z = z;
        has_previous = true;
        ++count;
    }

    return count;
}

void append_cubic_bspline_curve(const Abc::P3fArraySamplePtr& positions, const size_t offset, const size_t num_vertices, const size_t output_count, const Abc::FloatArraySamplePtr& knots, const size_t knot_offset, const Abc::M44d& transform, std::vector<float>& points) {
    if (knots) {
        const float* const knot = knots->get() + knot_offset;
        const double min_u = knot[3];
        const double max_u = knot[num_vertices];
        if (max_u <= min_u) {
            throw std::runtime_error("Cubic B-spline Alembic curve has an invalid knot domain");
        }
        for (size_t i = 0; i < output_count; ++i) {
            const double u = (i == output_count - 1)
                ? max_u
                : min_u + (static_cast<double>(i) * (max_u - min_u) / static_cast<double>(output_count - 1));
            append_transformed_point(evaluate_cubic_bspline_with_knots(positions, offset, num_vertices, knots, knot_offset, u), transform, points);
        }
        return;
    }

    const size_t span_count = num_vertices - 3;
    for (size_t i = 0; i < output_count; ++i) {
        const double u = static_cast<double>(i) * static_cast<double>(span_count) / static_cast<double>(output_count - 1);
        const size_t span = std::min(static_cast<size_t>(std::floor(u)), span_count - 1);
        const double t = (i == output_count - 1) ? 1.0 : u - static_cast<double>(span);
        append_transformed_point(evaluate_cubic_bspline(positions, offset, span, t), transform, points);
    }
}

size_t output_point_count_for_curve(const std::string& path, const AbcGeom::ICurvesSchema::sample_type& sample, const size_t num_vertices) {
    if (sample.getWrap() != AbcGeom::kNonPeriodic) {
        throw_unsupported_curve_schema(path, sample);
    }

    if (sample.getType() == AbcGeom::kLinear) {
        if (num_vertices < 2) {
            throw std::runtime_error(fmt::format("Alembic curve at \"{}\" has fewer than 2 vertices", path));
        }
        return num_vertices;
    }

    if (sample.getType() == AbcGeom::kCubic && sample.getBasis() == AbcGeom::kBsplineBasis) {
        if (num_vertices < 4) {
            throw std::runtime_error(fmt::format("Cubic B-spline Alembic curve at \"{}\" has fewer than 4 CVs", path));
        }
        if (globals::abc_load_tess_factor == 0) {
            throw std::runtime_error("--abc-load-tess-factor must be at least 1");
        }
        if (num_vertices > std::numeric_limits<size_t>::max() / globals::abc_load_tess_factor) {
            throw std::runtime_error(fmt::format("Cubic B-spline Alembic curve at \"{}\" has too many output points", path));
        }
        return num_vertices * static_cast<size_t>(globals::abc_load_tess_factor);
    }

    throw_unsupported_curve_schema(path, sample);
}

void load_curves(const int depth, const std::string& path, const Abc::IObject& obj, const Abc::M44d& transform, std::vector<float>& points, std::vector<unsigned short>& segments) {
    const std::string spaces(depth * 2, ' ');
    AbcGeom::ICurves curves(obj, Abc::kWrapExisting);
    AbcGeom::ICurvesSchema::sample_type sample;
    curves.getSchema().get(sample);

    const auto positions = sample.getPositions();
    const auto curves_num_vertices = sample.getCurvesNumVertices();
    if (!positions || !curves_num_vertices) {
        throw std::runtime_error(fmt::format("Alembic curve schema at \"{}\" is missing positions or curvesNumVertices", path));
    }
    if (sample.getPositionWeights() || sample.getOrders()) {
        throw_unsupported_curve_schema(path, sample);
    }
    const auto knots = sample.getKnots();

    const size_t num_curves = sample.getNumCurves();
    const size_t num_points = positions->size();

    log_info("{}  num curves: {}", spaces, num_curves);
    log_info("{}  num points: {}", spaces, num_points);

    segments.reserve(segments.size() + num_curves);

    size_t input_offset = 0;
    size_t knot_offset = 0;
    std::vector<size_t> output_counts;
    std::vector<size_t> knot_offsets;
    output_counts.reserve(num_curves);
    if (knots) knot_offsets.reserve(num_curves);
    for (size_t i = 0; i < num_curves; ++i) {
        const int num_vertices_int = (*curves_num_vertices)[i];
        if (num_vertices_int < 0) {
            throw std::runtime_error(fmt::format("Alembic curve at \"{}\" has a negative vertex count", path));
        }
        const size_t num_vertices = static_cast<size_t>(num_vertices_int);
        input_offset += num_vertices;

        const size_t output_count = output_point_count_for_curve(path, sample, num_vertices);
        if (output_count - 1 > std::numeric_limits<unsigned short>::max()) {
            throw std::runtime_error(fmt::format("Alembic curve at \"{}\" has {} output segments, exceeding the maximum limit of {}",
                                                 path, output_count - 1, std::numeric_limits<unsigned short>::max()));
        }
        output_counts.push_back(output_count);

        if (knots) {
            const size_t knot_count = num_vertices + 4;
            if (knot_offset + knot_count > knots->size()) {
                throw std::runtime_error(fmt::format("Alembic curve at \"{}\" has too few knots", path));
            }
            const float* const knot = knots->get() + knot_offset;
            for (size_t k = 1; k < knot_count; ++k) {
                if (knot[k] < knot[k - 1]) {
                    throw std::runtime_error(fmt::format("Alembic curve at \"{}\" has a decreasing knot vector", path));
                }
            }
            knot_offsets.push_back(knot_offset);
            knot_offset += knot_count;
        }
    }

    if (input_offset != num_points) {
        throw std::runtime_error(fmt::format("Sum of curvesNumVertices does not match number of points at \"{}\"! {} vs {}", path, input_offset, num_points));
    }
    if (knots && knot_offset != knots->size()) {
        throw std::runtime_error(fmt::format("Sum of expected knot counts does not match number of knots at \"{}\"! {} vs {}", path, knot_offset, knots->size()));
    }

    input_offset = 0;
    for (size_t i = 0; i < num_curves; ++i) {
        const size_t num_vertices = static_cast<size_t>((*curves_num_vertices)[i]);
        const size_t output_count = output_counts[i];

        if (sample.getType() == AbcGeom::kLinear) {
            segments.push_back(static_cast<unsigned short>(output_count - 1));
            append_linear_curve(positions, input_offset, num_vertices, transform, points);
        } else {
            std::vector<float> curve_points;
            curve_points.reserve(output_count * 3);
            append_cubic_bspline_curve(positions, input_offset, num_vertices, output_count, knots, knots ? knot_offsets[i] : 0, transform, curve_points);
            const size_t deduplicated_count = append_deduplicated_points(curve_points, points);
            if (deduplicated_count >= 2) {
                segments.push_back(static_cast<unsigned short>(deduplicated_count - 1));
            } else {
                points.resize(points.size() - deduplicated_count * 3);
            }
        }

        input_offset += num_vertices;
    }
}

void load_abc_sub(int depth, const std::string& path, const Abc::IObject& obj, const Abc::M44d& parent_transform, std::vector<float>& points, std::vector<unsigned short>& segments) {
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
        load_curves(depth, path, obj, current_transform, points, segments);
    }

    for (size_t i = 0; i < obj.getNumChildren(); i++) {
        const Abc::IObject child = obj.getChild(i);
        load_abc_sub(depth + 1, path + "/" + child.getName(), child, current_transform, points, segments);
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
    load_abc_sub(0, archive.getTop().getName(), archive.getTop(), identity, points, segments);

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
