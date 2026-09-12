#include <mesh_gen/MeshGeneration.h>

#include "MeshMath.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <unordered_set>
#include <vector>

namespace mesh_gen {
namespace {

using namespace math;

inline constexpr Vec3f x_axis{1.0f, 0.0f, 0.0f};
inline constexpr Vec3f y_axis{0.0f, 1.0f, 0.0f};
inline constexpr Vec3f z_axis{0.0f, 0.0f, 1.0f};
inline constexpr auto pi{std::numbers::pi_v<float>};
inline constexpr auto two_pi{2.0f * pi};
inline constexpr std::int32_t hex_side_count{6};

void append_quad_with_uvs(MeshData& mesh_data,
                          Vec3f const first,
                          Vec3f const second,
                          Vec3f const third,
                          Vec3f const fourth,
                          Vec3f const normal,
                          std::array<Vec2f, 4> const& uvs) {
    auto const base_index{static_cast<std::uint32_t>(mesh_data.positions.size())};
    mesh_data.positions.insert(mesh_data.positions.end(), {first, second, third, fourth});
    mesh_data.normals.insert(mesh_data.normals.end(), {normal, normal, normal, normal});
    mesh_data.uvs.insert(mesh_data.uvs.end(), uvs.begin(), uvs.end());
    mesh_data.indices.insert(
        mesh_data.indices.end(),
        {base_index, base_index + 2, base_index + 1, base_index, base_index + 3, base_index + 2});
}

void append_quad(MeshData& mesh_data,
                 Vec3f const first,
                 Vec3f const second,
                 Vec3f const third,
                 Vec3f const fourth,
                 Vec3f const normal) {
    append_quad_with_uvs(mesh_data,
                         first,
                         second,
                         third,
                         fourth,
                         normal,
                         {{{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}}});
}

void add_box_face(MeshData& mesh_data,
                  Vec3f const normal,
                  Vec3f const bottom_left,
                  Vec3f const bottom_right,
                  Vec3f const top_right,
                  Vec3f const top_left) {
    append_quad(mesh_data, bottom_left, bottom_right, top_right, top_left, normal);
}

void add_projected_polygon(MeshData& mesh_data,
                           Vec3f const supplied_normal,
                           std::vector<Vec3f> positions,
                           bool const derive_normal) {
    assert(positions.size() >= 3);
    auto normal{derive_normal
                    ? normalize(cross(positions[1] - positions[0], positions[2] - positions[0]))
                    : supplied_normal};
    Vec3f center{};
    for (auto const position : positions) {
        center = center + position;
    }
    center = center * (1.0f / static_cast<float>(positions.size()));

    auto const reversed{
        derive_normal
            ? dot(normal, center) < 0.0f
            : dot(cross(positions[1] - positions[0], positions[2] - positions[0]), normal) < 0.0f};
    if (reversed) {
        std::ranges::reverse(positions);
        if (derive_normal) {
            normal = -normal;
        }
    }

    auto const tangent_reference{std::abs(normal.z) < 0.9f ? z_axis : y_axis};
    auto const tangent{normalize(cross(tangent_reference, normal))};
    auto const bitangent{cross(normal, tangent)};
    Vec2f uv_min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec2f uv_max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    std::vector<Vec2f> projected_uvs;
    projected_uvs.reserve(positions.size());
    for (auto const position : positions) {
        auto const uv{Vec2f{dot(position, tangent), dot(position, bitangent)}};
        projected_uvs.push_back(uv);
        uv_min.x = std::min(uv_min.x, uv.x);
        uv_min.y = std::min(uv_min.y, uv.y);
        uv_max.x = std::max(uv_max.x, uv.x);
        uv_max.y = std::max(uv_max.y, uv.y);
    }

    auto const uv_size{uv_max - uv_min};
    auto const base_index{static_cast<std::uint32_t>(mesh_data.positions.size())};
    auto const vertex_count{positions.size()};
    for (std::size_t vertex_index{}; vertex_index < vertex_count; ++vertex_index) {
        auto const uv{projected_uvs[vertex_index] - uv_min};
        mesh_data.positions.push_back(positions[vertex_index]);
        mesh_data.normals.push_back(normal);
        mesh_data.uvs.push_back({uv_size.x > small_number ? uv.x / uv_size.x : 0.0f,
                                 uv_size.y > small_number ? uv.y / uv_size.y : 0.0f});
    }
    for (std::size_t vertex_index{1}; vertex_index < vertex_count - 1; ++vertex_index) {
        mesh_data.indices.insert(mesh_data.indices.end(),
                                 {base_index,
                                  base_index + static_cast<std::uint32_t>(vertex_index + 1),
                                  base_index + static_cast<std::uint32_t>(vertex_index)});
    }
}

void add_beveled_box_face(MeshData& mesh_data,
                          Vec3f const center,
                          Vec3f const normal,
                          Vec3f const horizontal,
                          Vec3f const vertical,
                          float const half_width,
                          float const half_height,
                          float const bevel_width) {
    std::array<Vec2f, 8> const outline{{{-half_width + bevel_width, -half_height},
                                        {half_width - bevel_width, -half_height},
                                        {half_width, -half_height + bevel_width},
                                        {half_width, half_height - bevel_width},
                                        {half_width - bevel_width, half_height},
                                        {-half_width + bevel_width, half_height},
                                        {-half_width, half_height - bevel_width},
                                        {-half_width, -half_height + bevel_width}}};
    std::vector<Vec3f> positions;
    positions.reserve(outline.size());
    for (auto const point : outline) {
        positions.push_back(center + horizontal * point.x + vertical * point.y);
    }
    add_projected_polygon(mesh_data, normal, std::move(positions), false);
}

void add_cylinder_side(MeshData& mesh_data, CylinderParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    for (std::int32_t segment_index{}; segment_index <= parameters.radial_segments;
         ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{two_pi * fraction};
        auto const cosine{std::cos(angle)};
        auto const sine{std::sin(angle)};
        Vec3f const normal{cosine, sine, 0.0f};

        mesh_data.positions.push_back(
            {parameters.radius * cosine, parameters.radius * sine, -half_height});
        mesh_data.positions.push_back(
            {parameters.radius * cosine, parameters.radius * sine, half_height});
        mesh_data.normals.insert(mesh_data.normals.end(), {normal, normal});
        mesh_data.uvs.insert(mesh_data.uvs.end(), {{fraction, 1.0f}, {fraction, 0.0f}});
    }

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const bottom{static_cast<std::uint32_t>(segment_index * 2)};
        auto const top{bottom + 1};
        auto const next_bottom{bottom + 2};
        auto const next_top{bottom + 3};
        mesh_data.indices.insert(mesh_data.indices.end(),
                                 {bottom, next_top, next_bottom, bottom, top, next_top});
    }
}

void add_cylinder_cap(MeshData& mesh_data,
                      CylinderParameters const& parameters,
                      float const z,
                      Vec3f const normal) {
    auto const center_index{static_cast<std::uint32_t>(mesh_data.positions.size())};
    mesh_data.positions.push_back({0.0f, 0.0f, z});
    mesh_data.normals.push_back(normal);
    mesh_data.uvs.push_back({0.5f, 0.5f});

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{two_pi * fraction};
        auto const cosine{std::cos(angle)};
        auto const sine{std::sin(angle)};
        mesh_data.positions.push_back({parameters.radius * cosine, parameters.radius * sine, z});
        mesh_data.normals.push_back(normal);
        mesh_data.uvs.push_back({0.5f + cosine * 0.5f, 0.5f + sine * 0.5f});
    }

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const current{center_index + 1 + static_cast<std::uint32_t>(segment_index)};
        auto const next{
            center_index + 1 +
            static_cast<std::uint32_t>((segment_index + 1) % parameters.radial_segments)};
        if (normal.z > 0.0f) {
            mesh_data.indices.insert(mesh_data.indices.end(), {center_index, next, current});
        } else {
            mesh_data.indices.insert(mesh_data.indices.end(), {center_index, current, next});
        }
    }
}

[[nodiscard]] auto cone_side_normal(ConeParameters const& parameters, float const angle) -> Vec3f {
    return normalize({parameters.height * std::cos(angle),
                      parameters.height * std::sin(angle),
                      parameters.radius});
}

void add_cone_side(MeshData& mesh_data, ConeParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    for (std::int32_t segment_index{}; segment_index <= parameters.radial_segments;
         ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{two_pi * fraction};
        mesh_data.positions.push_back({parameters.radius * std::cos(angle),
                                       parameters.radius * std::sin(angle),
                                       -half_height});
        mesh_data.normals.push_back(cone_side_normal(parameters, angle));
        mesh_data.uvs.push_back({fraction, 1.0f});
    }

    auto const apex_start{static_cast<std::uint32_t>(mesh_data.positions.size())};
    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const fraction{(static_cast<float>(segment_index) + 0.5f) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{two_pi * fraction};
        mesh_data.positions.push_back({0.0f, 0.0f, half_height});
        mesh_data.normals.push_back(cone_side_normal(parameters, angle));
        mesh_data.uvs.push_back({fraction, 0.0f});
    }

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const current{static_cast<std::uint32_t>(segment_index)};
        mesh_data.indices.insert(mesh_data.indices.end(),
                                 {current, apex_start + current, current + 1});
    }
}

void add_cone_base(MeshData& mesh_data, ConeParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    auto const center_index{static_cast<std::uint32_t>(mesh_data.positions.size())};
    mesh_data.positions.push_back({0.0f, 0.0f, -half_height});
    mesh_data.normals.push_back(-z_axis);
    mesh_data.uvs.push_back({0.5f, 0.5f});

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{two_pi * fraction};
        auto const cosine{std::cos(angle)};
        auto const sine{std::sin(angle)};
        mesh_data.positions.push_back(
            {parameters.radius * cosine, parameters.radius * sine, -half_height});
        mesh_data.normals.push_back(-z_axis);
        mesh_data.uvs.push_back({0.5f + cosine * 0.5f, 0.5f + sine * 0.5f});
    }

    for (std::int32_t segment_index{}; segment_index < parameters.radial_segments;
         ++segment_index) {
        auto const current{center_index + 1 + static_cast<std::uint32_t>(segment_index)};
        auto const next{
            center_index + 1 +
            static_cast<std::uint32_t>((segment_index + 1) % parameters.radial_segments)};
        mesh_data.indices.insert(mesh_data.indices.end(), {center_index, current, next});
    }
}

[[nodiscard]] auto make_ring(float const radius, float const z, float const angle_offset)
    -> std::array<Vec3f, hex_side_count> {
    std::array<Vec3f, hex_side_count> positions{};
    for (std::int32_t side_index{}; side_index < hex_side_count; ++side_index) {
        auto const angle{angle_offset + two_pi * static_cast<float>(side_index) /
                                            static_cast<float>(hex_side_count)};
        positions[static_cast<std::size_t>(side_index)] = {
            radius * std::cos(angle), radius * std::sin(angle), z};
    }
    return positions;
}

void add_hex_tile_triangle(MeshData& mesh_data,
                           Vec3f const first,
                           Vec3f const second,
                           Vec3f const third,
                           Vec3f const normal,
                           float const uv_radius) {
    auto const make_uv = [uv_radius](Vec3f const position) {
        return Vec2f{position.x / (2.0f * uv_radius) + 0.5f,
                     position.y / (2.0f * uv_radius) + 0.5f};
    };
    auto const base_index{static_cast<std::uint32_t>(mesh_data.positions.size())};
    mesh_data.positions.insert(mesh_data.positions.end(), {first, second, third});
    mesh_data.normals.insert(mesh_data.normals.end(), {normal, normal, normal});
    mesh_data.uvs.insert(mesh_data.uvs.end(), {make_uv(first), make_uv(second), make_uv(third)});
    mesh_data.indices.insert(mesh_data.indices.end(), {base_index, base_index + 2, base_index + 1});
}

struct EdgeKey {
    std::int64_t midpoint_x{};
    std::int64_t midpoint_y{};
    std::int32_t orientation{};

    [[nodiscard]] auto operator==(EdgeKey const&) const -> bool = default;
};

struct EdgeKeyHash {
    [[nodiscard]] auto operator()(EdgeKey const& key) const noexcept -> std::size_t {
        auto result{std::hash<std::int64_t>{}(key.midpoint_x)};
        result ^= std::hash<std::int64_t>{}(key.midpoint_y) + 0x9e3779b9U + (result << 6U) +
                  (result >> 2U);
        result ^= std::hash<std::int32_t>{}(key.orientation) + 0x9e3779b9U + (result << 6U) +
                  (result >> 2U);
        return result;
    }
};

void add_wall_segment(MeshData& mesh_data,
                      Vec2f const start,
                      Vec2f const end,
                      float const wall_thickness,
                      float const depth) {
    auto const direction{normalize(end - start)};
    auto const perpendicular{Vec2f{-direction.y, direction.x}};
    auto const half_width{wall_thickness * 0.5f};
    auto const half_depth{depth * 0.5f};
    auto const extended_start{start - direction * half_width};
    auto const extended_end{end + direction * half_width};
    auto const start_left{extended_start + perpendicular * half_width};
    auto const start_right{extended_start - perpendicular * half_width};
    auto const end_left{extended_end + perpendicular * half_width};
    auto const end_right{extended_end - perpendicular * half_width};

    auto const make_position = [](Vec2f const position, float const z) {
        return Vec3f{position.x, position.y, z};
    };
    auto const top_start_left{make_position(start_left, half_depth)};
    auto const top_start_right{make_position(start_right, half_depth)};
    auto const top_end_left{make_position(end_left, half_depth)};
    auto const top_end_right{make_position(end_right, half_depth)};
    auto const bottom_start_left{make_position(start_left, -half_depth)};
    auto const bottom_start_right{make_position(start_right, -half_depth)};
    auto const bottom_end_left{make_position(end_left, -half_depth)};
    auto const bottom_end_right{make_position(end_right, -half_depth)};
    auto const direction_3d{Vec3f{direction.x, direction.y, 0.0f}};
    auto const perpendicular_3d{Vec3f{perpendicular.x, perpendicular.y, 0.0f}};

    append_quad(mesh_data, top_start_right, top_end_right, top_end_left, top_start_left, z_axis);
    append_quad(mesh_data,
                bottom_start_left,
                bottom_end_left,
                bottom_end_right,
                bottom_start_right,
                -z_axis);
    append_quad(mesh_data,
                bottom_start_left,
                top_start_left,
                top_end_left,
                bottom_end_left,
                perpendicular_3d);
    append_quad(mesh_data,
                bottom_start_right,
                bottom_end_right,
                top_end_right,
                top_start_right,
                -perpendicular_3d);
    append_quad(mesh_data,
                bottom_start_left,
                bottom_start_right,
                top_start_right,
                top_start_left,
                -direction_3d);
    append_quad(
        mesh_data, bottom_end_right, bottom_end_left, top_end_left, top_end_right, direction_3d);
}

[[nodiscard]] auto make_cell_center(std::int32_t const row,
                                    std::int32_t const column,
                                    float const cell_radius,
                                    bool const pointy_top) -> Vec2f {
    auto const root_three{std::sqrt(3.0f)};
    if (pointy_top) {
        return {root_three * cell_radius *
                    (static_cast<float>(column) + 0.5f * static_cast<float>(row & 1)),
                1.5f * cell_radius * static_cast<float>(row)};
    }
    return {1.5f * cell_radius * static_cast<float>(column),
            root_three * cell_radius *
                (static_cast<float>(row) + 0.5f * static_cast<float>(column & 1))};
}

[[nodiscard]] auto make_edge_key(Vec2f const start, Vec2f const end, std::int32_t const side_index)
    -> EdgeKey {
    constexpr double quantization_scale{1000.0};
    auto const midpoint{(start + end) * 0.5f};
    return {std::llround(static_cast<double>(midpoint.x) * quantization_scale),
            std::llround(static_cast<double>(midpoint.y) * quantization_scale),
            side_index % 3};
}

}

auto generate_box(BoxParameters const& parameters) -> MeshData {
    MeshData mesh_data;
    mesh_data.positions.reserve(24);
    mesh_data.normals.reserve(24);
    mesh_data.uvs.reserve(24);
    mesh_data.indices.reserve(36);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const x{half_dimensions.x};
    auto const y{half_dimensions.y};
    auto const z{half_dimensions.z};

    add_box_face(mesh_data, x_axis, {x, -y, -z}, {x, y, -z}, {x, y, z}, {x, -y, z});
    add_box_face(mesh_data, -x_axis, {-x, y, -z}, {-x, -y, -z}, {-x, -y, z}, {-x, y, z});
    add_box_face(mesh_data, y_axis, {-x, y, -z}, {-x, y, z}, {x, y, z}, {x, y, -z});
    add_box_face(mesh_data, -y_axis, {x, -y, -z}, {x, -y, z}, {-x, -y, z}, {-x, -y, -z});
    add_box_face(mesh_data, z_axis, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z});
    add_box_face(mesh_data, -z_axis, {-x, y, -z}, {x, y, -z}, {x, -y, -z}, {-x, -y, -z});
    return mesh_data;
}

auto generate_beveled_box(BeveledBoxParameters const& parameters) -> MeshData {
    assert(minimum_component(parameters.dimensions) > 0.0f);
    assert(parameters.bevel_width > 0.0f);
    assert(parameters.bevel_width < minimum_component(parameters.dimensions) * 0.5f);

    MeshData mesh_data;
    mesh_data.positions.reserve(120);
    mesh_data.normals.reserve(120);
    mesh_data.uvs.reserve(120);
    mesh_data.indices.reserve(204);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const x{half_dimensions.x};
    auto const y{half_dimensions.y};
    auto const z{half_dimensions.z};
    auto const bevel{parameters.bevel_width};

    add_beveled_box_face(mesh_data, {x, 0.0f, 0.0f}, x_axis, y_axis, z_axis, y, z, bevel);
    add_beveled_box_face(mesh_data, {-x, 0.0f, 0.0f}, -x_axis, -y_axis, z_axis, y, z, bevel);
    add_beveled_box_face(mesh_data, {0.0f, y, 0.0f}, y_axis, -x_axis, z_axis, x, z, bevel);
    add_beveled_box_face(mesh_data, {0.0f, -y, 0.0f}, -y_axis, x_axis, z_axis, x, z, bevel);
    add_beveled_box_face(mesh_data, {0.0f, 0.0f, z}, z_axis, x_axis, y_axis, x, y, bevel);
    add_beveled_box_face(mesh_data, {0.0f, 0.0f, -z}, -z_axis, x_axis, -y_axis, x, y, bevel);

    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const y_sign : {-1.0f, 1.0f}) {
            auto const normal{normalize(Vec3f{x_sign, y_sign, 0.0f})};
            add_projected_polygon(mesh_data,
                                  normal,
                                  {{x_sign * x, y_sign * (y - bevel), -z + bevel},
                                   {x_sign * (x - bevel), y_sign * y, -z + bevel},
                                   {x_sign * (x - bevel), y_sign * y, z - bevel},
                                   {x_sign * x, y_sign * (y - bevel), z - bevel}},
                                  false);
        }
    }
    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const z_sign : {-1.0f, 1.0f}) {
            auto const normal{normalize(Vec3f{x_sign, 0.0f, z_sign})};
            add_projected_polygon(mesh_data,
                                  normal,
                                  {{x_sign * x, -y + bevel, z_sign * (z - bevel)},
                                   {x_sign * (x - bevel), -y + bevel, z_sign * z},
                                   {x_sign * (x - bevel), y - bevel, z_sign * z},
                                   {x_sign * x, y - bevel, z_sign * (z - bevel)}},
                                  false);
        }
    }
    for (float const y_sign : {-1.0f, 1.0f}) {
        for (float const z_sign : {-1.0f, 1.0f}) {
            auto const normal{normalize(Vec3f{0.0f, y_sign, z_sign})};
            add_projected_polygon(mesh_data,
                                  normal,
                                  {{-x + bevel, y_sign * y, z_sign * (z - bevel)},
                                   {-x + bevel, y_sign * (y - bevel), z_sign * z},
                                   {x - bevel, y_sign * (y - bevel), z_sign * z},
                                   {x - bevel, y_sign * y, z_sign * (z - bevel)}},
                                  false);
        }
    }
    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const y_sign : {-1.0f, 1.0f}) {
            for (float const z_sign : {-1.0f, 1.0f}) {
                auto const normal{normalize(Vec3f{x_sign, y_sign, z_sign})};
                add_projected_polygon(mesh_data,
                                      normal,
                                      {{x_sign * x, y_sign * (y - bevel), z_sign * (z - bevel)},
                                       {x_sign * (x - bevel), y_sign * y, z_sign * (z - bevel)},
                                       {x_sign * (x - bevel), y_sign * (y - bevel), z_sign * z}},
                                      false);
            }
        }
    }
    return mesh_data;
}

auto generate_wedge(WedgeParameters const& parameters) -> MeshData {
    assert(minimum_component(parameters.dimensions) > 0.0f);
    assert(parameters.top_length > 0.0f);
    assert(parameters.top_length <= parameters.dimensions.x);
    assert(std::abs(parameters.top_offset) + parameters.top_length * 0.5f <=
           parameters.dimensions.x * 0.5f);

    MeshData mesh_data;
    mesh_data.positions.reserve(24);
    mesh_data.normals.reserve(24);
    mesh_data.uvs.reserve(24);
    mesh_data.indices.reserve(36);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const top_half_length{parameters.top_length * 0.5f};
    auto const bottom_left_x{-half_dimensions.x};
    auto const bottom_right_x{half_dimensions.x};
    auto const top_left_x{parameters.top_offset - top_half_length};
    auto const top_right_x{parameters.top_offset + top_half_length};
    auto const front_y{-half_dimensions.y};
    auto const back_y{half_dimensions.y};
    auto const bottom_z{-half_dimensions.z};
    auto const top_z{half_dimensions.z};

    Vec3f const front_bottom_left{bottom_left_x, front_y, bottom_z};
    Vec3f const front_bottom_right{bottom_right_x, front_y, bottom_z};
    Vec3f const front_top_left{top_left_x, front_y, top_z};
    Vec3f const front_top_right{top_right_x, front_y, top_z};
    Vec3f const back_bottom_left{bottom_left_x, back_y, bottom_z};
    Vec3f const back_bottom_right{bottom_right_x, back_y, bottom_z};
    Vec3f const back_top_left{top_left_x, back_y, top_z};
    Vec3f const back_top_right{top_right_x, back_y, top_z};

    add_projected_polygon(mesh_data,
                          {},
                          {front_bottom_left, front_top_left, front_top_right, front_bottom_right},
                          true);
    add_projected_polygon(
        mesh_data, {}, {back_bottom_left, back_bottom_right, back_top_right, back_top_left}, true);
    add_projected_polygon(
        mesh_data,
        {},
        {front_bottom_left, front_bottom_right, back_bottom_right, back_bottom_left},
        true);
    add_projected_polygon(
        mesh_data, {}, {front_top_left, back_top_left, back_top_right, front_top_right}, true);
    add_projected_polygon(
        mesh_data, {}, {front_bottom_left, back_bottom_left, back_top_left, front_top_left}, true);
    add_projected_polygon(mesh_data,
                          {},
                          {front_bottom_right, front_top_right, back_top_right, back_bottom_right},
                          true);
    return mesh_data;
}

auto generate_cylinder(CylinderParameters const& parameters) -> MeshData {
    assert(parameters.radius > 0.0f);
    assert(parameters.height > 0.0f);
    assert(parameters.radial_segments >= 3);

    MeshData mesh_data;
    auto const vertex_count{parameters.radial_segments * 4 + 4};
    auto const index_count{parameters.radial_segments * 12};
    mesh_data.positions.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.normals.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.uvs.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.indices.reserve(static_cast<std::size_t>(index_count));
    add_cylinder_side(mesh_data, parameters);
    auto const half_height{parameters.height * 0.5f};
    add_cylinder_cap(mesh_data, parameters, half_height, z_axis);
    add_cylinder_cap(mesh_data, parameters, -half_height, -z_axis);
    return mesh_data;
}

auto generate_cone(ConeParameters const& parameters) -> MeshData {
    assert(parameters.radius > 0.0f);
    assert(parameters.height > 0.0f);
    assert(parameters.radial_segments >= 3);

    MeshData mesh_data;
    auto const vertex_count{parameters.radial_segments * 3 + 2};
    auto const index_count{parameters.radial_segments * 6};
    mesh_data.positions.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.normals.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.uvs.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.indices.reserve(static_cast<std::size_t>(index_count));
    add_cone_side(mesh_data, parameters);
    add_cone_base(mesh_data, parameters);
    return mesh_data;
}

auto generate_sphere(SphereParameters const& parameters) -> MeshData {
    assert(parameters.radius > 0.0f);
    assert(parameters.longitude_segments >= 3);
    assert(parameters.latitude_segments >= 2);

    MeshData mesh_data;
    auto const vertices_per_row{parameters.longitude_segments + 1};
    auto const vertex_count{(parameters.latitude_segments + 1) * vertices_per_row};
    auto const index_count{parameters.longitude_segments * (parameters.latitude_segments - 1) * 6};
    mesh_data.positions.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.normals.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.uvs.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.indices.reserve(static_cast<std::size_t>(index_count));

    for (std::int32_t latitude_index{}; latitude_index <= parameters.latitude_segments;
         ++latitude_index) {
        auto const v{static_cast<float>(latitude_index) /
                     static_cast<float>(parameters.latitude_segments)};
        auto const latitude_angle{pi * v};
        auto const radial_distance{std::sin(latitude_angle)};
        auto const z{std::cos(latitude_angle)};

        for (std::int32_t longitude_index{}; longitude_index <= parameters.longitude_segments;
             ++longitude_index) {
            auto const u{static_cast<float>(longitude_index) /
                         static_cast<float>(parameters.longitude_segments)};
            auto const longitude_angle{two_pi * u};
            auto const normal{normalize({radial_distance * std::cos(longitude_angle),
                                         radial_distance * std::sin(longitude_angle),
                                         z})};
            mesh_data.positions.push_back(normal * parameters.radius);
            mesh_data.normals.push_back(normal);
            mesh_data.uvs.push_back({u, v});
        }
    }

    for (std::int32_t latitude_index{}; latitude_index < parameters.latitude_segments;
         ++latitude_index) {
        for (std::int32_t longitude_index{}; longitude_index < parameters.longitude_segments;
             ++longitude_index) {
            auto const upper_left{
                static_cast<std::uint32_t>(latitude_index * vertices_per_row + longitude_index)};
            auto const upper_right{upper_left + 1};
            auto const lower_left{upper_left + static_cast<std::uint32_t>(vertices_per_row)};
            auto const lower_right{lower_left + 1};
            if (latitude_index == 0) {
                mesh_data.indices.insert(mesh_data.indices.end(),
                                         {upper_left, lower_right, lower_left});
            } else if (latitude_index == parameters.latitude_segments - 1) {
                mesh_data.indices.insert(mesh_data.indices.end(),
                                         {upper_left, upper_right, lower_left});
            } else {
                mesh_data.indices.insert(
                    mesh_data.indices.end(),
                    {upper_left, lower_right, lower_left, upper_left, upper_right, lower_right});
            }
        }
    }
    return mesh_data;
}

auto generate_hex_tile(HexTileParameters const& parameters) -> MeshData {
    assert(parameters.outer_radius > 0.0f);
    assert(parameters.depth > 0.0f);
    assert(parameters.bevel_width > 0.0f);
    assert(parameters.bevel_width < parameters.outer_radius);
    assert(parameters.bevel_width < parameters.depth * 0.5f);

    constexpr std::int32_t face_triangle_count{hex_side_count * 2};
    constexpr std::int32_t surface_quad_count{hex_side_count * 3};
    MeshData mesh_data;
    auto const vertex_count{face_triangle_count * 3 + surface_quad_count * 4};
    mesh_data.positions.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.normals.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.uvs.reserve(static_cast<std::size_t>(vertex_count));
    mesh_data.indices.reserve(
        static_cast<std::size_t>(face_triangle_count * 3 + surface_quad_count * 6));

    auto const half_depth{parameters.depth * 0.5f};
    auto const face_radius{parameters.outer_radius - parameters.bevel_width};
    auto const wall_half_depth{half_depth - parameters.bevel_width};
    auto const angle_offset{parameters.pointy_top ? pi * 0.5f : 0.0f};
    auto const top_face{make_ring(face_radius, half_depth, angle_offset)};
    auto const top_wall{make_ring(parameters.outer_radius, wall_half_depth, angle_offset)};
    auto const bottom_wall{make_ring(parameters.outer_radius, -wall_half_depth, angle_offset)};
    auto const bottom_face{make_ring(face_radius, -half_depth, angle_offset)};
    Vec3f const top_center{0.0f, 0.0f, half_depth};
    Vec3f const bottom_center{0.0f, 0.0f, -half_depth};

    for (std::int32_t side_index{}; side_index < hex_side_count; ++side_index) {
        auto const next_index{(side_index + 1) % hex_side_count};
        auto const current{static_cast<std::size_t>(side_index)};
        auto const next{static_cast<std::size_t>(next_index)};
        add_hex_tile_triangle(mesh_data,
                              top_center,
                              top_face[current],
                              top_face[next],
                              z_axis,
                              parameters.outer_radius);
        add_hex_tile_triangle(mesh_data,
                              bottom_center,
                              bottom_face[next],
                              bottom_face[current],
                              -z_axis,
                              parameters.outer_radius);
        append_quad(mesh_data,
                    top_face[current],
                    top_wall[current],
                    top_wall[next],
                    top_face[next],
                    normalize(cross(top_wall[current] - top_face[current],
                                    top_wall[next] - top_face[current])));
        append_quad(mesh_data,
                    top_wall[current],
                    bottom_wall[current],
                    bottom_wall[next],
                    top_wall[next],
                    normalize(cross(bottom_wall[current] - top_wall[current],
                                    bottom_wall[next] - top_wall[current])));
        append_quad(mesh_data,
                    bottom_wall[current],
                    bottom_face[current],
                    bottom_face[next],
                    bottom_wall[next],
                    normalize(cross(bottom_face[current] - bottom_wall[current],
                                    bottom_face[next] - bottom_wall[current])));
    }
    return mesh_data;
}

auto generate_hex_frame(HexFrameParameters const& parameters) -> MeshData {
    assert(parameters.outer_radius > 0.0f);
    assert(parameters.wall_thickness > 0.0f);
    assert(parameters.wall_thickness < parameters.outer_radius);
    assert(parameters.depth > 0.0f);

    MeshData mesh_data;
    constexpr std::int32_t surface_count{4};
    auto const quad_count{hex_side_count * surface_count};
    mesh_data.positions.reserve(static_cast<std::size_t>(quad_count * 4));
    mesh_data.normals.reserve(static_cast<std::size_t>(quad_count * 4));
    mesh_data.uvs.reserve(static_cast<std::size_t>(quad_count * 4));
    mesh_data.indices.reserve(static_cast<std::size_t>(quad_count * 6));

    auto const inner_radius{parameters.outer_radius - parameters.wall_thickness};
    auto const half_depth{parameters.depth * 0.5f};
    auto const angle_offset{parameters.pointy_top ? pi * 0.5f : 0.0f};
    auto const outer_front{make_ring(parameters.outer_radius, half_depth, angle_offset)};
    auto const outer_back{make_ring(parameters.outer_radius, -half_depth, angle_offset)};
    auto const inner_front{make_ring(inner_radius, half_depth, angle_offset)};
    auto const inner_back{make_ring(inner_radius, -half_depth, angle_offset)};

    for (std::int32_t side_index{}; side_index < hex_side_count; ++side_index) {
        auto const next_index{(side_index + 1) % hex_side_count};
        auto const current{static_cast<std::size_t>(side_index)};
        auto const next{static_cast<std::size_t>(next_index)};
        auto face_normal{normalize(outer_front[current] + outer_front[next])};
        face_normal.z = 0.0f;
        face_normal = normalize(face_normal);
        if (parameters.uv_mode == HexFrameUvMode::perimeter) {
            auto const u0{static_cast<float>(side_index) / static_cast<float>(hex_side_count)};
            auto const u1{static_cast<float>(side_index + 1) / static_cast<float>(hex_side_count)};
            constexpr float front_outer_v{0.1f};
            constexpr float front_inner_v{0.9f};
            append_quad_with_uvs(mesh_data,
                                 outer_front[current],
                                 outer_front[next],
                                 inner_front[next],
                                 inner_front[current],
                                 z_axis,
                                 {{{u0, front_outer_v},
                                   {u1, front_outer_v},
                                   {u1, front_inner_v},
                                   {u0, front_inner_v}}});
            append_quad_with_uvs(mesh_data,
                                 outer_back[current],
                                 inner_back[current],
                                 inner_back[next],
                                 outer_back[next],
                                 -z_axis,
                                 {{{u0, front_outer_v},
                                   {u0, front_inner_v},
                                   {u1, front_inner_v},
                                   {u1, front_outer_v}}});
            append_quad_with_uvs(
                mesh_data,
                outer_back[current],
                outer_back[next],
                outer_front[next],
                outer_front[current],
                face_normal,
                {{{u0, 0.0f}, {u1, 0.0f}, {u1, front_outer_v}, {u0, front_outer_v}}});
            append_quad_with_uvs(
                mesh_data,
                inner_back[current],
                inner_front[current],
                inner_front[next],
                inner_back[next],
                -face_normal,
                {{{u0, 1.0f}, {u0, front_inner_v}, {u1, front_inner_v}, {u1, 1.0f}}});
            continue;
        }
        append_quad(mesh_data,
                    outer_front[current],
                    outer_front[next],
                    inner_front[next],
                    inner_front[current],
                    z_axis);
        append_quad(mesh_data,
                    outer_back[current],
                    inner_back[current],
                    inner_back[next],
                    outer_back[next],
                    -z_axis);
        append_quad(mesh_data,
                    outer_back[current],
                    outer_back[next],
                    outer_front[next],
                    outer_front[current],
                    face_normal);
        append_quad(mesh_data,
                    inner_back[current],
                    inner_front[current],
                    inner_front[next],
                    inner_back[next],
                    -face_normal);
    }
    return mesh_data;
}

auto generate_honeycomb_panel(HoneycombPanelParameters const& parameters) -> MeshData {
    assert(parameters.rows > 0);
    assert(parameters.columns > 0);
    assert(parameters.cell_radius > 0.0f);
    assert(parameters.wall_thickness > 0.0f);
    assert(parameters.wall_thickness < parameters.cell_radius);
    assert(parameters.depth > 0.0f);

    std::vector<Vec2f> centers;
    centers.reserve(static_cast<std::size_t>(parameters.rows * parameters.columns));
    Vec2f minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec2f maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (std::int32_t row{}; row < parameters.rows; ++row) {
        for (std::int32_t column{}; column < parameters.columns; ++column) {
            auto const center{
                make_cell_center(row, column, parameters.cell_radius, parameters.pointy_top)};
            centers.push_back(center);
            minimum.x = std::min(minimum.x, center.x);
            minimum.y = std::min(minimum.y, center.y);
            maximum.x = std::max(maximum.x, center.x);
            maximum.y = std::max(maximum.y, center.y);
        }
    }
    auto const panel_center{(minimum + maximum) * 0.5f};

    auto const maximum_edge_count{parameters.rows * parameters.columns * hex_side_count};
    constexpr std::int32_t faces_per_segment{6};
    MeshData mesh_data;
    mesh_data.positions.reserve(
        static_cast<std::size_t>(maximum_edge_count * faces_per_segment * 4));
    mesh_data.normals.reserve(mesh_data.positions.capacity());
    mesh_data.uvs.reserve(mesh_data.positions.capacity());
    mesh_data.indices.reserve(static_cast<std::size_t>(maximum_edge_count * faces_per_segment * 6));

    std::unordered_set<EdgeKey, EdgeKeyHash> generated_edges;
    auto const angle_offset{parameters.pointy_top ? pi * 0.5f : 0.0f};
    for (auto const uncentered_cell_center : centers) {
        auto const cell_center{uncentered_cell_center - panel_center};
        std::array<Vec2f, hex_side_count> corners{};
        for (std::int32_t side_index{}; side_index < hex_side_count; ++side_index) {
            auto const angle{angle_offset + two_pi * static_cast<float>(side_index) /
                                                static_cast<float>(hex_side_count)};
            corners[static_cast<std::size_t>(side_index)] =
                cell_center + Vec2f{parameters.cell_radius * std::cos(angle),
                                    parameters.cell_radius * std::sin(angle)};
        }

        for (std::int32_t side_index{}; side_index < hex_side_count; ++side_index) {
            auto const next_index{(side_index + 1) % hex_side_count};
            auto const start{corners[static_cast<std::size_t>(side_index)]};
            auto const end{corners[static_cast<std::size_t>(next_index)]};
            auto const edge_key{make_edge_key(start, end, side_index)};
            if (generated_edges.contains(edge_key)) {
                continue;
            }
            generated_edges.insert(edge_key);
            add_wall_segment(mesh_data, start, end, parameters.wall_thickness, parameters.depth);
        }
    }
    return mesh_data;
}

}
