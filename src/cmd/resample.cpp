#include "cmd.h"

using namespace Eigen;

namespace {
struct {
    float& target_segment_length = cmd::param::f("resample", "target_segment_length");
    bool& linear_subdiv = cmd::param::b("resample", "linear_subdiv");
    bool& catmull_rom = cmd::param::b("resample", "catmull_rom");
    float& cr_power = cmd::param::f("resample", "cr_power");
    bool& c2_interp = cmd::param::b("resample", "c2_interp");
} param;
template <typename T>
inline T lerp(const T& a, const T& b, float t) {
    return (1.0f - t) * a + t * b;
}
inline float sq(float x) { return x * x; }
void push_back_3f(std::vector<float>& vec, const Vector3f& v) {
    vec.push_back(v[0]);
    vec.push_back(v[1]);
    vec.push_back(v[2]);
}
struct Circle {
    Vector3f center;
    Vector3f axis1;
    Vector3f axis2;
    std::array<float, 3> limits;
};
Circle c2i_getCircle(const Vector3f& p_prev, const Vector3f& p_curr, const Vector3f& p_next) {
    const float smallAngle = 0.01f;
    Vector3f vec1 = p_curr - p_prev;
    Vector3f vec2 = p_next - p_curr;
    Vector3f mid1 = p_prev + vec1 / 2.f;
    Vector3f mid2 = p_curr + vec2 / 2.f;
    Vector3f vec1_cross_vec2 = vec1.cross(vec2);
    Vector3f n = vec1_cross_vec2 / std::max(vec1_cross_vec2.norm(), 0.0001f);
    Vector3f dir1 = n.cross(vec1);
    Vector3f dir2 = n.cross(vec2);
    float det = vec1_cross_vec2.dot(n);
    if (std::abs(det) < 0.001) {
        if (vec1.dot(vec2) >= 0) {
            const float s = std::sin(smallAngle);
            const float l1 = vec1.norm();
            const float l2 = vec2.norm();
            return {
                .center = p_curr,
                .axis1 = Vector3f::Zero(),
                .axis2 = vec2 / s,
                .limits = {-smallAngle * l1 / l2, 0.f, smallAngle}
            };
        } else {
            det = 0.001;
        }
    }
    float s = (mid2 - mid1).cross(dir2).dot(n) / det;
    Vector3f center = mid1 + s * dir1;
    Vector3f axis1 = p_curr - center;
    Vector3f axis2 = n.cross(axis1);
    Vector3f toPt2 = p_next - center;
    float limit2 = std::atan2(axis2.dot(toPt2), axis1.dot(toPt2));
    Vector3f toPt1 = p_prev - center;
    float limit1 = std::atan2(axis2.dot(toPt1), axis1.dot(toPt1));
    if (limit1 * limit2 > 0) {
        if (std::abs(limit1) < std::abs(limit2)) limit2 += limit2 > 0 ? -2 * globals::pi : 2 * globals::pi;
        if (std::abs(limit1) > std::abs(limit2)) limit1 += limit1 > 0 ? -2 * globals::pi : 2 * globals::pi;
    }
    return {
        .center = center,
        .axis1 = axis1,
        .axis2 = axis2,
        .limits = {limit1, 0.f, limit2}
    };
}
Circle c2i_getEllipse(const Vector3f& p_prev, const Vector3f& p_curr, const Vector3f& p_next) {
    const int numIter = 16;
    Vector3f vec1 = p_prev - p_curr;
    Vector3f vec2 = p_next - p_curr;
    float len1 = vec1.norm();
    float len2 = vec2.norm();
    float cosa = vec1.dot(vec2) / (len1 * len2);
    float maxA = std::acos(cosa);
    float ang = maxA * 0.5f;
    float incA = maxA * 0.25f;
    float l1 = len1;
    float l2 = len2;
    if (len1 < len2) { std::swap(l1, l2); }
    float a, b, c, d;
    for (int iter = 0; iter < numIter; iter++) {
        float theta = ang * 0.5f;
        a = l1 * std::sin(theta);
        b = l1 * std::cos(theta);
        float beta = maxA - theta;
        c = l2 * std::sin(beta);
        d = l2 * std::cos(beta);
        float v = sq(1 - d / b) + sq(c / a); // ellipse equation
        ang += (v > 1) ? incA : -incA;
        incA *= 0.5f;
    }
    Vector3f vec, pt2;
    float len;
    if (len1 < len2) {
        vec = vec2;
        len = len2;
        pt2 = p_next;
    } else {
        vec = vec1;
        len = len1;
        pt2 = p_prev;
    }
    Vector3f dir = vec / len;
    Vector3f vec1_cross_vec2 = vec1.cross(vec2);
    Vector3f n = vec1_cross_vec2 / std::max(vec1_cross_vec2.norm(), 0.0001f);
    Vector3f perp = n.cross(dir);
    float cross = vec1_cross_vec2.dot(n);
    if ((len1 < len2 && cross > 0) || (len1 >= len2 && cross < 0)) perp = -perp;
    float v = b * b / len;
    float h = b * a / len;
    Vector3f axis1 = -v * dir - h * perp;
    Vector3f center = p_curr - axis1;
    Vector3f axis2 = pt2 - center;
    float beta = std::asin(std::min(c / a, 1.f));
    return {
        .center = center,
        .axis1 = axis1,
        .axis2 = (len1 < len2) ? axis2 : -axis2,
        .limits = (len1 < len2) ? std::array<float, 3>{ -beta, 0.f, globals::pi_2 } : std::array<float, 3>{ -globals::pi_2, 0.f, beta }
    };
}
Vector3f c2i_curvePos(const Circle& curve, float t, bool first_half) {
    float tt = first_half ? lerp(curve.limits[0], curve.limits[1], t) : lerp(curve.limits[1], curve.limits[2], t);
    Vector3f p = curve.center + curve.axis1 * std::cos(tt) + curve.axis2 * std::sin(tt);
    return p;
}
Vector3f c2i_interpolate(const Circle& curve1, const Circle& curve2, float t) {
    Vector3f p1 = c2i_curvePos(curve1, t, false);
    Vector3f p2 = c2i_curvePos(curve2, t, true);
    float theta = t * globals::pi_2;
    Vector3f p = sq(std::cos(theta)) * p1 + sq(std::sin(theta)) * p2;
    return p;
}
}

void cmd::parse::resample(args::Subparser &parser) {
    args::ValueFlag<float> target_segment_length(parser, "R", "(REQUIRED) Target segment length (0 uses average segment length)", {"target-segment-length", 'l'}, args::Options::Required);
    args::Flag linear_subdiv(parser, "linear-subdiv", "Use linear subdivision", {"linear-subdiv"});
    args::Flag catmull_rom(parser, "catmull-rom", "Use parameterized Catmull-Rom interpolation", {"catmull-rom"});
    args::ValueFlag<float> cr_power(parser, "R", "Power parameter for Catmull-Rom (default: 0.5)", {"cr-power"}, 0.5f);
    args::Flag c2_interp(parser, "c2-interp", "Use hybrid C2-interpolating spline", {"c2-interp"});
    parser.Parse();
    globals::cmd_exec = cmd::exec::resample;
    globals::output_file_wo_ext = [](){ return fmt::format("{}_resampled_tsl_{}{}", globals::input_file_wo_ext, ::param.target_segment_length, ::param.linear_subdiv ? "_ls" : ::param.catmull_rom ? fmt::format("_cr{}", ::param.cr_power) : ""); };
    globals::check_error = [](){
        if (::param.target_segment_length < 0) {
            throw std::runtime_error(fmt::format("Invalid target segment length: {}", ::param.target_segment_length));
        }
        if (::param.target_segment_length == 0 && (::param.linear_subdiv || ::param.catmull_rom || ::param.c2_interp)) {
            throw std::runtime_error("When --target-segment-length is 0, none of --linear-subdiv, --catmull-rom, or --c2-interp can be specified.");
        }
        if (::param.linear_subdiv + ::param.catmull_rom + ::param.c2_interp > 1) {
            throw std::runtime_error("Flags --linear-subdiv and --catmull-rom simultaneously and --c2-interp are mutually exclusive.");
        }
    };
    ::param.target_segment_length = *target_segment_length;
    ::param.linear_subdiv = linear_subdiv;
    ::param.catmull_rom = catmull_rom;
    ::param.cr_power = *cr_power;
    ::param.c2_interp = c2_interp;
}

std::shared_ptr<cyHairFile> cmd::exec::resample(std::shared_ptr<cyHairFile> hairfile_in) {
    const auto& header_in = hairfile_in->GetHeader();

    const bool has_segments = hairfile_in->GetSegmentsArray() != nullptr;
    const bool has_thickness = hairfile_in->GetThicknessArray() != nullptr;
    const bool has_transparency = hairfile_in->GetTransparencyArray() != nullptr;
    const bool has_color = hairfile_in->GetColorsArray() != nullptr;

    const unsigned int hair_count = header_in.hair_count;

    std::vector<std::vector<float>> points_per_strand(hair_count);
    std::vector<std::vector<float>> thickness_per_strand(hair_count);
    std::vector<std::vector<float>> transparency_per_strand(hair_count);
    std::vector<std::vector<float>> color_per_strand(hair_count);

    unsigned int offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        if (i > 0 && i % 100 == 0)
            log_debug("Processing hair {}/{}", i, hair_count);

        const unsigned int num_segments = has_segments ? hairfile_in->GetSegmentsArray()[i] : header_in.d_segments;

        std::vector<unsigned int> j_range(num_segments);
        std::iota(j_range.begin(), j_range.end(), 0);

        // Determine number of subsegments for each segment
        std::vector<float> segment_length(num_segments);
        for (unsigned int j : j_range) {
            const Vector3f p0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j));
            const Vector3f p1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j + 1));
            segment_length[j] = (p1 - p0).norm();
        }

        // Preparation for catmull-rom mode
        std::vector<float> segment_length_pow;
        std::transform(segment_length.begin(), segment_length.end(), std::back_inserter(segment_length_pow), [](float l) { return std::pow(l, ::param.cr_power); });
        std::vector<float> knots;
        std::partial_sum(segment_length_pow.begin(), segment_length_pow.end(), std::back_inserter(knots));
        knots.insert(knots.begin(), 0);

        // Preparation for c2-interp mode
        std::optional<Circle> curve1;

        if (::param.linear_subdiv || ::param.catmull_rom || ::param.c2_interp) {
            std::vector<unsigned int> num_subsegments_per_segment(num_segments);
            for (unsigned int j : j_range) {
                num_subsegments_per_segment[j] = (unsigned int)std::ceil(segment_length[j] / ::param.target_segment_length);
            }

            const unsigned int num_subsegments_total = std::accumulate(num_subsegments_per_segment.begin(), num_subsegments_per_segment.end(), 0);

            // Allocate memory
            points_per_strand[i].reserve(3 * (num_subsegments_total + 1));
            if (has_thickness) thickness_per_strand[i].reserve(num_subsegments_total + 1);
            if (has_transparency) transparency_per_strand[i].reserve(num_subsegments_total + 1);
            if (has_color) color_per_strand[i].reserve(3 * (num_subsegments_total + 1));

            for (unsigned int j : j_range) {
                const Vector3f point0 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j));
                const Vector3f point1 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j + 1));

                const std::optional<float> thickness0 = has_thickness ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + j]) : std::nullopt;
                const std::optional<float> thickness1 = has_thickness ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + j + 1]) : std::nullopt;
                const std::optional<float> transparency0 = has_transparency ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + j]) : std::nullopt;
                const std::optional<float> transparency1 = has_transparency ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + j + 1]) : std::nullopt;
                const std::optional<Vector3f> color0 = has_color ? std::optional<Vector3f>(Map<Vector3f>(hairfile_in->GetColorsArray() + 3*(offset + j))) : std::nullopt;
                const std::optional<Vector3f> color1 = has_color ? std::optional<Vector3f>(Map<Vector3f>(hairfile_in->GetColorsArray() + 3*(offset + j + 1))) : std::nullopt;

                // Add first point
                if (j == 0) {
                    push_back_3f(points_per_strand[i], point0);
                    if (has_thickness) thickness_per_strand[i].push_back(*thickness0);
                    if (has_transparency) transparency_per_strand[i].push_back(*transparency0);
                    if (has_color) push_back_3f(color_per_strand[i], *color0);
                }
                Vector3f p_last = Map<Vector3f>(&points_per_strand[i].back() - 2);

                if (::param.linear_subdiv || num_segments == 1) {
                    for (unsigned int k = 0; k < num_subsegments_per_segment[j]; ++k) {
                        const float t = (k + 1) / (float)num_subsegments_per_segment[j];
                        push_back_3f(points_per_strand[i], lerp(point0, point1, t));
                        if (has_thickness) thickness_per_strand[i].push_back(lerp(*thickness0, *thickness1, t));
                        if (has_transparency) transparency_per_strand[i].push_back(lerp(*transparency0, *transparency1, t));
                        if (has_color) push_back_3f(color_per_strand[i], lerp(*color0, *color1, t));
                    }
                } else if (::param.catmull_rom) {
                    auto cr_interpolate_1f = [&offset, &num_segments, &knots, &j](const float t, float * const array) -> float {
                        const float t0 = (j == 0) ? -knots[1] : knots[j - 1];
                        const float t1 = knots[j];
                        const float t2 = knots[j + 1];
                        const float t3 = (j == num_segments - 1) ? (2 * knots[j + 1] - knots[j]) : knots[j + 2];
                        const float p1 = array[offset + j];
                        const float p2 = array[offset + j + 1];
                        const float p0 = (j == 0) ? 2 * p1 - p2 : array[offset + j - 1];
                        const float p3 = (j == num_segments - 1) ? 2 * p2 - p1 : array[offset + j + 2];
                        const float A1 = (t1 - t) / (t1 - t0) * p0 + (t - t0) / (t1 - t0) * p1;
                        const float A2 = (t2 - t) / (t2 - t1) * p1 + (t - t1) / (t2 - t1) * p2;
                        const float A3 = (t3 - t) / (t3 - t2) * p2 + (t - t2) / (t3 - t2) * p3;
                        const float B1 = (t2 - t) / (t2 - t0) * A1 + (t - t0) / (t2 - t0) * A2;
                        const float B2 = (t3 - t) / (t3 - t1) * A2 + (t - t1) / (t3 - t1) * A3;
                        const float C = (t2 - t) / (t2 - t1) * B1 + (t - t1) / (t2 - t1) * B2;
                        return C;                    
                    };
                    auto cr_interpolate_3f = [&offset, &num_segments, &knots, &j](const float t, float * const array) -> Vector3f {
                        const float t0 = (j == 0) ? -knots[1] : knots[j - 1];
                        const float t1 = knots[j];
                        const float t2 = knots[j + 1];
                        const float t3 = (j == num_segments - 1) ? (2 * knots[j + 1] - knots[j]) : knots[j + 2];
                        const Vector3f p1 = Map<Vector3f>(array + 3 * (offset + j));
                        const Vector3f p2 = Map<Vector3f>(array + 3 * (offset + j + 1));
                        const Vector3f p0 = (j == 0) ? Vector3f(2 * p1 - p2) : Map<Vector3f>(array + 3 * (offset + j - 1));
                        const Vector3f p3 = (j == num_segments - 1) ? Vector3f(2 * p2 - p1) : Map<Vector3f>(array + 3 * (offset + j + 2));
                        const Vector3f A1 = (t1 - t) / (t1 - t0) * p0 + (t - t0) / (t1 - t0) * p1;
                        const Vector3f A2 = (t2 - t) / (t2 - t1) * p1 + (t - t1) / (t2 - t1) * p2;
                        const Vector3f A3 = (t3 - t) / (t3 - t2) * p2 + (t - t2) / (t3 - t2) * p3;
                        const Vector3f B1 = (t2 - t) / (t2 - t0) * A1 + (t - t0) / (t2 - t0) * A2;
                        const Vector3f B2 = (t3 - t) / (t3 - t1) * A2 + (t - t1) / (t3 - t1) * A3;
                        const Vector3f C = (t2 - t) / (t2 - t1) * B1 + (t - t1) / (t2 - t1) * B2;
                        return C;
                    };
                    float t = knots[j];
                    while (true) {
                        float dt = 1e-4f;
                        Vector3f p;
                        while (true) {
                            p = cr_interpolate_3f(t + dt, hairfile_in->GetPointsArray());
                            if ((p - p_last).norm() >= ::param.target_segment_length) break;
                            dt *= 1.1f;
                            if (dt > knots[j + 1] - knots[j]) break;
                        }
                        t += dt;
                        if (t >= knots[j + 1]) break;
                        push_back_3f(points_per_strand[i], p);
                        p_last = p;
                        if (has_thickness) thickness_per_strand[i].push_back(cr_interpolate_1f(t, hairfile_in->GetThicknessArray()));
                        if (has_transparency) transparency_per_strand[i].push_back(cr_interpolate_1f(t, hairfile_in->GetTransparencyArray()));
                        if (has_color) push_back_3f(color_per_strand[i], cr_interpolate_3f(t, hairfile_in->GetColorsArray()));
                    }
                    push_back_3f(points_per_strand[i], point1);
                    if (has_thickness) thickness_per_strand[i].push_back(*thickness1);
                    if (has_transparency) transparency_per_strand[i].push_back(*transparency1);
                    if (has_color) push_back_3f(color_per_strand[i], *color1);
                } else if (::param.c2_interp) {
                    Circle curve2;
                    if (j == num_segments - 1) {
                        curve2 = {
                            .center = curve1->center,
                            .axis1 = curve1->axis1,
                            .axis2 = curve1->axis2,
                            .limits = { 0.f, curve1->limits[2], curve1->limits[2] }
                        };
                    } else {
                        const Vector3f point2 = Map<Vector3f>(hairfile_in->GetPointsArray() + 3*(offset + j + 2));
                        curve2 = c2i_getCircle(point0, point1, point2);
                        float lim0 = curve2.limits[0];
                        float lim2 = curve2.limits[2];
                        if (lim2 < lim0) std::swap(lim0, lim2);
                        if (lim0 < -globals::pi_2 || lim2 > globals::pi_2) {
                            curve2 = c2i_getEllipse(point0, point1, point2);
                        }
                    }
                    if (!curve1) {
                        curve1 = {
                            .center = curve2.center,
                            .axis1 = curve2.axis1,
                            .axis2 = curve2.axis2,
                            .limits = { curve2.limits[0], curve2.limits[0], 0.f }
                        };
                    }
                    float t = 0.f;
                    while (true) {
                        float dt = 1e-4f;
                        Vector3f p;
                        while (true) {
                            p = c2i_interpolate(*curve1, curve2, t + dt);
                            if ((p - p_last).norm() >= ::param.target_segment_length) break;
                            dt *= 1.1f;
                            if (dt > 1.f) break;
                        }
                        t += dt;
                        if (t >= 1) break;
                        push_back_3f(points_per_strand[i], p);
                        p_last = p;
                        if (has_thickness) thickness_per_strand[i].push_back(lerp(*thickness0, *thickness1, t));
                        if (has_transparency) transparency_per_strand[i].push_back(lerp(*transparency0, *transparency1, t));
                        if (has_color) push_back_3f(color_per_strand[i], lerp(*color0, *color1, t));
                    }
                    push_back_3f(points_per_strand[i], point1);
                    if (has_thickness) thickness_per_strand[i].push_back(*thickness1);
                    if (has_transparency) transparency_per_strand[i].push_back(*transparency1);
                    if (has_color) push_back_3f(color_per_strand[i], *color1);
                    curve1 = curve2;
                }
            }
        } else {
            const unsigned int num_points = num_segments + 1;
            const double total_length = std::accumulate(segment_length.begin(), segment_length.end(), 0.0);
            const unsigned int target_num_points = ::param.target_segment_length ? static_cast<unsigned int>(std::ceil(total_length / ::param.target_segment_length)) + 1 : num_points;

            auto append_point = [&](const Vector3f& point,
                                    const std::optional<float>& thickness,
                                    const std::optional<float>& transparency,
                                    const std::optional<Vector3f>& color) {
                push_back_3f(points_per_strand[i], point);
                if (has_thickness) thickness_per_strand[i].push_back(*thickness);
                if (has_transparency) transparency_per_strand[i].push_back(*transparency);
                if (has_color) push_back_3f(color_per_strand[i], *color);
            };

            auto append_point_at = [&](unsigned int point_index) {
                const Vector3f point = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + point_index));
                const std::optional<float> thickness = has_thickness ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + point_index]) : std::nullopt;
                const std::optional<float> transparency = has_transparency ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + point_index]) : std::nullopt;
                const std::optional<Vector3f> color = has_color ? std::optional<Vector3f>(Map<const Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + point_index))) : std::nullopt;
                append_point(point, thickness, transparency, color);
            };

            points_per_strand[i].reserve(3 * target_num_points);
            if (has_thickness) thickness_per_strand[i].reserve(target_num_points);
            if (has_transparency) transparency_per_strand[i].reserve(target_num_points);
            if (has_color) color_per_strand[i].reserve(3 * target_num_points);

            std::vector<double> src_len_acc(num_points, 0.0);
            for (unsigned int j = 1; j < num_points; ++j)
                src_len_acc[j] = src_len_acc[j - 1] + segment_length[j - 1];

            const double tgt_len_segment = src_len_acc.back() / (target_num_points - 1.0);
            unsigned int src_i = 0;
            double tgt_len_acc = 0.0;
            unsigned int index = 0;

            append_point_at(0);
            ++index;

            while (true) {
                while (tgt_len_acc + tgt_len_segment <= src_len_acc[src_i]) {
                    tgt_len_acc += tgt_len_segment;
                    const double w1 = (tgt_len_acc - src_len_acc[src_i - 1]) / (src_len_acc[src_i] - src_len_acc[src_i - 1]);
                    const double w0 = 1.0 - w1;
                    const float w0f = static_cast<float>(w0);
                    const float w1f = static_cast<float>(w1);

                    const Vector3f point0 = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + src_i - 1));
                    const Vector3f point1 = Map<const Vector3f>(hairfile_in->GetPointsArray() + 3 * (offset + src_i));
                    const Vector3f point = point0 * w0f + point1 * w1f;

                    const std::optional<float> thickness = has_thickness
                        ? std::optional<float>(hairfile_in->GetThicknessArray()[offset + src_i - 1] * w0f + hairfile_in->GetThicknessArray()[offset + src_i] * w1f)
                        : std::nullopt;
                    const std::optional<float> transparency = has_transparency
                        ? std::optional<float>(hairfile_in->GetTransparencyArray()[offset + src_i - 1] * w0f + hairfile_in->GetTransparencyArray()[offset + src_i] * w1f)
                        : std::nullopt;
                    const std::optional<Vector3f> color = has_color
                        ? std::optional<Vector3f>(
                            Map<const Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + src_i - 1)) * w0f +
                            Map<const Vector3f>(hairfile_in->GetColorsArray() + 3 * (offset + src_i)) * w1f)
                        : std::nullopt;

                    append_point(point, thickness, transparency, color);
                    ++index;
                    if (index == target_num_points - 1) break;
                }

                if (index == target_num_points - 1) break;

                while (src_len_acc[src_i] <= tgt_len_acc + tgt_len_segment) {
                    ++src_i;
                }
            }
            append_point_at(num_points - 1);
        }

        offset += num_segments + 1;
    }

    const unsigned int num_points_total = std::accumulate(points_per_strand.begin(), points_per_strand.end(), 0,
        [](unsigned int sum, const std::vector<float>& v){ return sum + v.size() / 3; }
    );

    // Create output hair file
    std::shared_ptr<cyHairFile> hairfile_out = std::make_shared<cyHairFile>();

    // Copy header via memcpy
    std::memcpy((void*)&hairfile_out->GetHeader(), &header_in, sizeof(cyHairFile::Header));

    // Set hair_count & point_count, allocate arrays
    hairfile_out->SetHairCount(hair_count);
    hairfile_out->SetPointCount(num_points_total);
    hairfile_out->SetArrays(header_in.arrays | _CY_HAIR_FILE_SEGMENTS_BIT);

    // Copy data to arrays of hairfile_out
    offset = 0;
    for (unsigned int i = 0; i < hair_count; ++i) {
        const size_t num_points_per_strand = points_per_strand[i].size() / 3;
        if (num_points_per_strand - 1 > std::numeric_limits<unsigned short>::max()) {
            throw std::runtime_error(fmt::format("Number of segments per strand {} exceeds the maximum limit: {}", num_points_per_strand - 1, std::numeric_limits<unsigned short>::max()));
        }

        hairfile_out->GetSegmentsArray()[i] = num_points_per_strand - 1;

        std::memcpy(hairfile_out->GetPointsArray() + 3*offset, points_per_strand[i].data(), 3*num_points_per_strand*sizeof(float));

        if (has_thickness) std::memcpy(hairfile_out->GetThicknessArray() + offset, thickness_per_strand[i].data(), num_points_per_strand*sizeof(float));
        if (has_transparency) std::memcpy(hairfile_out->GetTransparencyArray() + offset, transparency_per_strand[i].data(), num_points_per_strand*sizeof(float));
        if (has_color) std::memcpy(hairfile_out->GetColorsArray() + 3*offset, color_per_strand[i].data(), 3*num_points_per_strand*sizeof(float));

        offset += num_points_per_strand;
    }

    return hairfile_out;
}
