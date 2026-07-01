#include <gtest/gtest.h>

#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#include "cmd.h"
#include "io.h"

namespace {

std::shared_ptr<cyHairFile> generate_test_data(bool uniform_segments = false) {
    // Prepare data
    const auto segments_array = uniform_segments ? std::vector<unsigned short>{ 5, 5, 5, 5, 5 } : std::vector<unsigned short>{ 3, 4, 5, 6, 7 };
    const unsigned int hair_count = segments_array.size();

    const unsigned int point_count = std::accumulate(segments_array.begin(), segments_array.end(), hair_count);

    std::vector<float> points_array(point_count * 3);
    std::vector<float> thickness_array(point_count);
    std::vector<float> transparency_array(point_count);
    std::vector<float> colors_array(point_count * 3);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        const unsigned short segments = segments_array[i];
        for (unsigned short j = 0; j <= segments; ++j) {
            points_array[3*(offset+j) + 0] = i;
            points_array[3*(offset+j) + 1] = j;
            points_array[3*(offset+j) + 2] = 0;

            thickness_array[offset+j] = 0.1f * (1 + i) * (1 + j);

            transparency_array[offset+j] = 0.5f;

            colors_array[3*(offset+j) + 0] = (i + 1.0f) / hair_count;
            colors_array[3*(offset+j) + 1] = (j + 1.0f) / segments;
            colors_array[3*(offset+j) + 2] = 0.5f;
        }
        offset += segments + 1;
    }

    // Store in the cyHairFile structure
    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    hairfile->SetArrays(
        _CY_HAIR_FILE_SEGMENTS_BIT |
        _CY_HAIR_FILE_POINTS_BIT |
        _CY_HAIR_FILE_THICKNESS_BIT |
        _CY_HAIR_FILE_TRANSPARENCY_BIT |
        _CY_HAIR_FILE_COLORS_BIT
    );
    hairfile->SetHairCount(hair_count);
    hairfile->SetPointCount(point_count);

    std::memcpy(hairfile->GetSegmentsArray(), segments_array.data(), sizeof(unsigned short) * hair_count);
    std::memcpy(hairfile->GetPointsArray(), points_array.data(), sizeof(float) * point_count * 3);
    std::memcpy(hairfile->GetThicknessArray(), thickness_array.data(), sizeof(float) * point_count);
    std::memcpy(hairfile->GetTransparencyArray(), transparency_array.data(), sizeof(float) * point_count);
    std::memcpy(hairfile->GetColorsArray(), colors_array.data(), sizeof(float) * point_count * 3);

    hairfile->SetDefaultSegmentCount(10);
    hairfile->SetDefaultThickness(0.1f);
    hairfile->SetDefaultTransparency(0.5f);
    hairfile->SetDefaultColor(0.25f, 0.5f, 0.75f);

    return hairfile;
}

template <typename Parent>
void write_curve(Parent& parent, const std::vector<Alembic::Abc::V3f>& positions) {
    Alembic::AbcGeom::OCurves curves(parent, "curves");
    Alembic::AbcGeom::OCurvesSchema::Sample sample;
    const std::vector<std::int32_t> nVertices = { static_cast<std::int32_t>(positions.size()) };
    sample.setPositions(positions);
    sample.setCurvesNumVertices(nVertices);
    sample.setType(Alembic::AbcGeom::kLinear);
    sample.setWrap(Alembic::AbcGeom::kNonPeriodic);
    sample.setBasis(Alembic::AbcGeom::kNoBasis);
    curves.getSchema().set(sample);
}

void write_curve(const std::string& filename,
                 const std::vector<Alembic::Abc::V3f>& positions,
                 const Alembic::AbcGeom::CurveType type,
                 const Alembic::AbcGeom::BasisType basis,
                 const Alembic::AbcGeom::CurvePeriodicity wrap = Alembic::AbcGeom::kNonPeriodic,
                 const std::vector<float> knots = {}) {
    Alembic::Abc::OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(), filename);
    Alembic::AbcGeom::OCurves curves(archive.getTop(), "curves");
    Alembic::AbcGeom::OCurvesSchema::Sample sample;
    const std::vector<std::int32_t> nVertices = { static_cast<std::int32_t>(positions.size()) };
    sample.setPositions(positions);
    sample.setCurvesNumVertices(nVertices);
    sample.setType(type);
    sample.setWrap(wrap);
    sample.setBasis(basis);
    if (!knots.empty()) {
        sample.setKnots(knots);
    }
    curves.getSchema().set(sample);
}

void expect_point(const cyHairFile& hairfile, size_t index, float x, float y, float z) {
    const float* points = hairfile.GetPointsArray();
    EXPECT_FLOAT_EQ(points[3*index + 0], x);
    EXPECT_FLOAT_EQ(points[3*index + 1], y);
    EXPECT_FLOAT_EQ(points[3*index + 2], z);
}

void expect_point_near(const cyHairFile& hairfile, size_t index, const Alembic::Abc::V3d& p) {
    const float* points = hairfile.GetPointsArray();
    EXPECT_NEAR(points[3*index + 0], p.x, 1e-5);
    EXPECT_NEAR(points[3*index + 1], p.y, 1e-5);
    EXPECT_NEAR(points[3*index + 2], p.z, 1e-5);
}

Alembic::Abc::V3d expected_cubic_bspline_point(const std::vector<Alembic::Abc::V3f>& cvs, const size_t index, const size_t output_count) {
    const size_t span_count = cvs.size() - 3;
    const double u = static_cast<double>(index) * static_cast<double>(span_count) / static_cast<double>(output_count - 1);
    const size_t span = std::min(static_cast<size_t>(std::floor(u)), span_count - 1);
    const double t = (index == output_count - 1) ? 1.0 : u - static_cast<double>(span);
    const double omt = 1.0 - t;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double b0 = omt * omt * omt / 6.0;
    const double b1 = (4.0 - 6.0 * t2 + 3.0 * t3) / 6.0;
    const double b2 = (1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3) / 6.0;
    const double b3 = t3 / 6.0;
    return Alembic::Abc::V3d(
        b0 * cvs[span + 0].x + b1 * cvs[span + 1].x + b2 * cvs[span + 2].x + b3 * cvs[span + 3].x,
        b0 * cvs[span + 0].y + b1 * cvs[span + 1].y + b2 * cvs[span + 2].y + b3 * cvs[span + 3].y,
        b0 * cvs[span + 0].z + b1 * cvs[span + 1].z + b2 * cvs[span + 2].z + b3 * cvs[span + 3].z
    );
}

}

TEST(io_abc, read) { auto hairfile = io::load_abc(TEST_DATA_DIR "/Bangs_100.abc"); }
TEST(io_bin, read) { auto hairfile = io::load_bin(TEST_DATA_DIR "/Bangs_100.bin"); }
TEST(io_data, read) { auto hairfile = io::load_data(TEST_DATA_DIR "/Bangs_100.data"); }
TEST(io_hair, read) { auto hairfile = io::load_hair(TEST_DATA_DIR "/Bangs_100.hair"); }
TEST(io_ma, read) { auto hairfile = io::load_ma(TEST_DATA_DIR "/Bangs_100.ma"); }
TEST(io_ply, read_ascii) { auto hairfile = io::load_ply(TEST_DATA_DIR "/Bangs_100_ascii.ply"); }
TEST(io_ply, read_binary) { auto hairfile = io::load_ply(TEST_DATA_DIR "/Bangs_100_binary.ply"); }
TEST(io_npy, read) { auto hairfile = io::load_npy(TEST_DATA_DIR "/base_0_idx_17453.npy"); }

TEST(io_abc, read_static_nested_transforms) {
    const std::string filename = "test_io_xform_nested.abc";
    {
        Alembic::Abc::OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(), filename);

        Alembic::AbcGeom::OXform parent(archive.getTop(), "parent");
        Alembic::AbcGeom::XformSample parent_sample;
        parent_sample.setTranslation(Alembic::Abc::V3d(10.0, 0.0, 0.0));
        parent.getSchema().set(parent_sample);

        Alembic::AbcGeom::OXform child(parent, "child");
        Alembic::AbcGeom::XformSample child_sample;
        child_sample.setScale(Alembic::Abc::V3d(2.0, 3.0, 4.0));
        child.getSchema().set(child_sample);

        write_curve(child, {
            Alembic::Abc::V3f(1.0f, 2.0f, 3.0f),
            Alembic::Abc::V3f(4.0f, 5.0f, 6.0f),
        });
    }

    globals::clear();
    auto hairfile = io::load_abc(filename);
    ASSERT_EQ(hairfile->GetHeader().hair_count, 1);
    ASSERT_EQ(hairfile->GetHeader().point_count, 2);
    ASSERT_EQ(hairfile->GetSegmentsArray()[0], 1);
    expect_point(*hairfile, 0, 12.0f, 6.0f, 12.0f);
    expect_point(*hairfile, 1, 18.0f, 15.0f, 24.0f);
}

TEST(io_abc, read_transform_without_inheritance) {
    const std::string filename = "test_io_xform_no_inherit.abc";
    {
        Alembic::Abc::OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(), filename);

        Alembic::AbcGeom::OXform parent(archive.getTop(), "parent");
        Alembic::AbcGeom::XformSample parent_sample;
        parent_sample.setTranslation(Alembic::Abc::V3d(10.0, 0.0, 0.0));
        parent.getSchema().set(parent_sample);

        Alembic::AbcGeom::OXform child(parent, "child");
        Alembic::AbcGeom::XformSample child_sample;
        child_sample.setInheritsXforms(false);
        child_sample.setScale(Alembic::Abc::V3d(2.0, 2.0, 2.0));
        child.getSchema().set(child_sample);

        write_curve(child, {
            Alembic::Abc::V3f(1.0f, 2.0f, 3.0f),
            Alembic::Abc::V3f(4.0f, 5.0f, 6.0f),
        });
    }

    globals::clear();
    auto hairfile = io::load_abc(filename);
    ASSERT_EQ(hairfile->GetHeader().hair_count, 1);
    ASSERT_EQ(hairfile->GetHeader().point_count, 2);
    expect_point(*hairfile, 0, 2.0f, 4.0f, 6.0f);
    expect_point(*hairfile, 1, 8.0f, 10.0f, 12.0f);
}

TEST(io_abc, read_animated_transform_uses_first_sample_and_warns) {
    const std::string filename = "test_io_xform_animated.abc";
    {
        Alembic::Abc::OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(), filename);

        Alembic::AbcGeom::OXform xform(archive.getTop(), "animated");
        Alembic::AbcGeom::XformSample sample;
        sample.setTranslation(Alembic::Abc::V3d(1.0, 0.0, 0.0));
        xform.getSchema().set(sample);
        sample.setTranslation(Alembic::Abc::V3d(100.0, 0.0, 0.0));
        xform.getSchema().set(sample);

        write_curve(xform, {
            Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
            Alembic::Abc::V3f(2.0f, 0.0f, 0.0f),
        });
    }

    globals::clear();
    auto hairfile = io::load_abc(filename);
    ASSERT_EQ(hairfile->GetHeader().hair_count, 1);
    ASSERT_EQ(hairfile->GetHeader().point_count, 2);
    expect_point(*hairfile, 0, 2.0f, 0.0f, 0.0f);
    expect_point(*hairfile, 1, 3.0f, 0.0f, 0.0f);

    ASSERT_TRUE(globals::json["log"]["warn"].is_array());
    ASSERT_EQ(globals::json["log"]["warn"].size(), 1);
    const std::string warning = globals::json["log"]["warn"][0].get<std::string>();
    EXPECT_NE(warning.find("Alembic transform \"animated\" has 2 samples"), std::string::npos);
    EXPECT_NE(warning.find("using the first frame and ignoring frames 2 through 2"), std::string::npos);
}

TEST(io_abc, read_cubic_bspline_default_tess_factor) {
    const std::string filename = "test_io_cubic_bspline_default.abc";
    const std::vector<Alembic::Abc::V3f> cvs = {
        Alembic::Abc::V3f(0.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(2.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(4.0f, 1.0f, 0.0f),
    };
    write_curve(filename, cvs, Alembic::AbcGeom::kCubic, Alembic::AbcGeom::kBsplineBasis, Alembic::AbcGeom::kNonPeriodic, {0, 0, 0, 0, 1, 2, 2, 2, 2});

    globals::clear();
    auto hairfile = io::load_abc(filename);
    const size_t expected_points = cvs.size() * 2;
    ASSERT_EQ(hairfile->GetHeader().hair_count, 1);
    ASSERT_EQ(hairfile->GetHeader().point_count, expected_points);
    ASSERT_EQ(hairfile->GetSegmentsArray()[0], expected_points - 1);
    expect_point_near(*hairfile, 0, Alembic::Abc::V3d(cvs.front().x, cvs.front().y, cvs.front().z));
    expect_point_near(*hairfile, expected_points - 1, Alembic::Abc::V3d(cvs.back().x, cvs.back().y, cvs.back().z));

    auto fixed = cmd::exec::autofix(hairfile);
    EXPECT_FALSE(fixed);
}

TEST(io_abc, read_cubic_bspline_custom_tess_factor) {
    const std::string filename = "test_io_cubic_bspline_custom.abc";
    const std::vector<Alembic::Abc::V3f> cvs = {
        Alembic::Abc::V3f(0.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(2.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(3.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(4.0f, 0.0f, 0.0f),
    };
    write_curve(filename, cvs, Alembic::AbcGeom::kCubic, Alembic::AbcGeom::kBsplineBasis);

    globals::clear();
    globals::abc_load_tess_factor = 3;
    auto hairfile = io::load_abc(filename);
    const size_t expected_points = cvs.size() * 3;
    ASSERT_EQ(hairfile->GetHeader().point_count, expected_points);
    ASSERT_EQ(hairfile->GetSegmentsArray()[0], expected_points - 1);
}

TEST(io_abc, read_cubic_bspline_tess_factor_one) {
    const std::string filename = "test_io_cubic_bspline_factor_one.abc";
    const std::vector<Alembic::Abc::V3f> cvs = {
        Alembic::Abc::V3f(0.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(2.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(3.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(4.0f, 0.0f, 0.0f),
    };
    write_curve(filename, cvs, Alembic::AbcGeom::kCubic, Alembic::AbcGeom::kBsplineBasis);

    globals::clear();
    globals::abc_load_tess_factor = 1;
    auto hairfile = io::load_abc(filename);
    const size_t expected_points = cvs.size();
    ASSERT_EQ(hairfile->GetHeader().point_count, expected_points);
    ASSERT_EQ(hairfile->GetSegmentsArray()[0], expected_points - 1);
    expect_point_near(*hairfile, 2, expected_cubic_bspline_point(cvs, 2, expected_points));
}

TEST(io_abc, read_unsupported_cubic_curve_schema_throws) {
    const std::string filename = "test_io_cubic_bezier_unsupported.abc";
    write_curve(filename, {
        Alembic::Abc::V3f(0.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(1.0f, 0.0f, 0.0f),
        Alembic::Abc::V3f(2.0f, 1.0f, 0.0f),
        Alembic::Abc::V3f(3.0f, 1.0f, 0.0f),
    }, Alembic::AbcGeom::kCubic, Alembic::AbcGeom::kBezierBasis);

    globals::clear();
    try {
        auto hairfile = io::load_abc(filename);
        (void)hairfile;
        FAIL() << "Expected unsupported cubic curve schema to throw";
    } catch (const std::runtime_error& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("Unsupported Alembic curve schema"), std::string::npos);
        EXPECT_NE(message.find("basis=bezier"), std::string::npos);
    }
}

TEST(io_abc, write) { auto hairfile = generate_test_data(); io::save_abc("test_io_out.abc", hairfile); }
TEST(io_bin, write) { auto hairfile = generate_test_data(); io::save_bin("test_io_out.bin", hairfile); }
TEST(io_data, write) { auto hairfile = generate_test_data(); io::save_data("test_io_out.data", hairfile); }
TEST(io_hair, write) { auto hairfile = generate_test_data(); io::save_hair("test_io_out.hair", hairfile); }
TEST(io_ma, write) { auto hairfile = generate_test_data(); io::save_ma("test_io_out.ma", hairfile); }
TEST(io_ply, write_ascii) { auto hairfile = generate_test_data(); globals::ply_save_ascii = true; io::save_ply("test_io_out_ascii.ply", hairfile); }
TEST(io_ply, write_binary) { auto hairfile = generate_test_data(); globals::ply_save_ascii = false; io::save_ply("test_io_out_binary.ply", hairfile); }
TEST(io_npy, write) { auto hairfile = generate_test_data(true); io::save_npy("test_io_out_binary.npy", hairfile); }
TEST(io_npy, write_fail) { auto hairfile = generate_test_data(false); EXPECT_THROW({ io::save_npy("test_io_out_binary.npy", hairfile); }, std::runtime_error); }

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
