#include <mesh_gen/MeshGeneration.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace mesh_gen::tests {
namespace {

[[nodiscard]] auto subtract(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] auto cross(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] auto dot(Vec3f const left, Vec3f const right) -> float {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] auto length_squared(Vec3f const value) -> float {
    return dot(value, value);
}

[[nodiscard]] auto nearly_equal(float const left, float const right, float const tolerance = 0.001f)
    -> bool {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] auto nearly_equal(Vec3f const left, Vec3f const right, float const tolerance = 0.001f)
    -> bool {
    return nearly_equal(left.x, right.x, tolerance) && nearly_equal(left.y, right.y, tolerance) &&
           nearly_equal(left.z, right.z, tolerance);
}

struct Bounds {
    Vec3f minimum{std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
    Vec3f maximum{std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest()};
};

[[nodiscard]] auto bounds_of(MeshData const& mesh) -> Bounds {
    Bounds bounds;
    for (auto const position : mesh.positions) {
        bounds.minimum.x = std::min(bounds.minimum.x, position.x);
        bounds.minimum.y = std::min(bounds.minimum.y, position.y);
        bounds.minimum.z = std::min(bounds.minimum.z, position.z);
        bounds.maximum.x = std::max(bounds.maximum.x, position.x);
        bounds.maximum.y = std::max(bounds.maximum.y, position.y);
        bounds.maximum.z = std::max(bounds.maximum.z, position.z);
    }
    return bounds;
}

void expect_valid_geometry(MeshData const& mesh) {
    ASSERT_TRUE(is_valid_mesh_data(mesh));
    for (auto const normal : mesh.normals) {
        EXPECT_TRUE(std::isfinite(normal.x));
        EXPECT_TRUE(std::isfinite(normal.y));
        EXPECT_TRUE(std::isfinite(normal.z));
        EXPECT_TRUE(nearly_equal(length_squared(normal), 1.0f));
    }
    for (auto const uv : mesh.uvs) {
        EXPECT_TRUE(std::isfinite(uv.x));
        EXPECT_TRUE(std::isfinite(uv.y));
        EXPECT_GE(uv.x, 0.0f);
        EXPECT_LE(uv.x, 1.0f);
        EXPECT_GE(uv.y, 0.0f);
        EXPECT_LE(uv.y, 1.0f);
    }

    auto const triangle_count{mesh.indices.size() / 3};
    for (std::size_t triangle_index{}; triangle_index < triangle_count; ++triangle_index) {
        auto const first_index{mesh.indices[triangle_index * 3]};
        auto const second_index{mesh.indices[triangle_index * 3 + 1]};
        auto const third_index{mesh.indices[triangle_index * 3 + 2]};
        auto const first{mesh.positions[first_index]};
        auto const second{mesh.positions[second_index]};
        auto const third{mesh.positions[third_index]};
        auto const unreal_front_normal{cross(subtract(third, first), subtract(second, first))};
        EXPECT_GT(length_squared(unreal_front_normal), 0.0f);
        EXPECT_GT(dot(unreal_front_normal, mesh.normals[first_index]), 0.0f);
    }
}

[[nodiscard]] auto same_mesh(MeshData const& left, MeshData const& right) -> bool {
    return left.positions == right.positions && left.normals == right.normals &&
           left.uvs == right.uvs && left.indices == right.indices &&
           left.triangle_material_roles == right.triangle_material_roles;
}

}

TEST(PrimitiveGenerators, ProduceExpectedBufferShapes) {
    auto const box{generate_box()};
    EXPECT_EQ(box.positions.size(), 24);
    EXPECT_EQ(box.indices.size(), 36);

    CylinderParameters const cylinder_parameters{30.0f, 80.0f, 12};
    auto const cylinder{generate_cylinder(cylinder_parameters)};
    EXPECT_EQ(cylinder.positions.size(), 52);
    EXPECT_EQ(cylinder.indices.size(), 144);

    ConeParameters const cone_parameters{30.0f, 80.0f, 12};
    auto const cone{generate_cone(cone_parameters)};
    EXPECT_EQ(cone.positions.size(), 38);
    EXPECT_EQ(cone.indices.size(), 72);

    SphereParameters const sphere_parameters{30.0f, 12, 6};
    auto const sphere{generate_sphere(sphere_parameters)};
    EXPECT_EQ(sphere.positions.size(), 91);
    EXPECT_EQ(sphere.indices.size(), 360);

    auto const hex_tile{generate_hex_tile()};
    EXPECT_EQ(hex_tile.positions.size(), 108);
    EXPECT_EQ(hex_tile.indices.size(), 144);

    auto const hex_frame{generate_hex_frame()};
    EXPECT_EQ(hex_frame.positions.size(), 96);
    EXPECT_EQ(hex_frame.indices.size(), 144);

    auto const honeycomb{generate_honeycomb_panel({1, 1, 40.0f, 6.0f, 10.0f, false})};
    EXPECT_EQ(honeycomb.positions.size(), 144);
    EXPECT_EQ(honeycomb.indices.size(), 216);

    auto const beveled_box{generate_beveled_box()};
    EXPECT_EQ(beveled_box.positions.size(), 120);
    EXPECT_EQ(beveled_box.indices.size(), 204);

    auto const wedge{generate_wedge()};
    EXPECT_EQ(wedge.positions.size(), 24);
    EXPECT_EQ(wedge.indices.size(), 36);
}

TEST(PrimitiveGenerators, ProduceValidConsistentlyWoundGeometry) {
    std::array<MeshData, 9> const meshes{generate_box(),
                                         generate_cylinder({30.0f, 80.0f, 12}),
                                         generate_sphere({30.0f, 12, 6}),
                                         generate_cone({30.0f, 80.0f, 12}),
                                         generate_hex_tile(),
                                         generate_hex_frame(),
                                         generate_honeycomb_panel({2, 2, 40.0f, 6.0f, 10.0f}),
                                         generate_beveled_box(),
                                         generate_wedge()};
    for (auto const& mesh : meshes) {
        expect_valid_geometry(mesh);
    }
}

TEST(PrimitiveGenerators, HonorRequestedBoundsAndOrientation) {
    auto const box_bounds{bounds_of(generate_box({{120.0f, 80.0f, 40.0f}}))};
    EXPECT_TRUE(nearly_equal(box_bounds.minimum, {-60.0f, -40.0f, -20.0f}));
    EXPECT_TRUE(nearly_equal(box_bounds.maximum, {60.0f, 40.0f, 20.0f}));

    auto const cylinder_bounds{bounds_of(generate_cylinder({30.0f, 80.0f, 12}))};
    EXPECT_TRUE(nearly_equal(cylinder_bounds.minimum, {-30.0f, -30.0f, -40.0f}));
    EXPECT_TRUE(nearly_equal(cylinder_bounds.maximum, {30.0f, 30.0f, 40.0f}));

    auto const sphere_bounds{bounds_of(generate_sphere({30.0f, 12, 6}))};
    EXPECT_TRUE(nearly_equal(sphere_bounds.minimum, {-30.0f, -30.0f, -30.0f}));
    EXPECT_TRUE(nearly_equal(sphere_bounds.maximum, {30.0f, 30.0f, 30.0f}));

    auto const beveled_bounds{bounds_of(generate_beveled_box({{120.0f, 80.0f, 40.0f}, 5.0f}))};
    EXPECT_TRUE(nearly_equal(beveled_bounds.minimum, {-60.0f, -40.0f, -20.0f}));
    EXPECT_TRUE(nearly_equal(beveled_bounds.maximum, {60.0f, 40.0f, 20.0f}));

    auto const flat_hex_bounds{bounds_of(generate_hex_frame({40.0f, 6.0f, 12.0f, false}))};
    auto const pointy_hex_bounds{bounds_of(generate_hex_frame({40.0f, 6.0f, 12.0f, true}))};
    EXPECT_TRUE(nearly_equal(flat_hex_bounds.maximum.x, 40.0f));
    EXPECT_TRUE(nearly_equal(pointy_hex_bounds.maximum.y, 40.0f));
    EXPECT_TRUE(nearly_equal(flat_hex_bounds.maximum.x, pointy_hex_bounds.maximum.y));
    EXPECT_TRUE(nearly_equal(flat_hex_bounds.maximum.y, pointy_hex_bounds.maximum.x));
}

TEST(HoneycombGenerator, SharesInternalEdgesAndLeavesCentersOpen) {
    auto const panel{generate_honeycomb_panel({2, 2, 40.0f, 6.0f, 12.0f, false})};
    EXPECT_EQ(panel.positions.size(), 19 * 24);
    EXPECT_EQ(panel.indices.size(), 19 * 36);
    auto const panel_bounds{bounds_of(panel)};
    EXPECT_TRUE(nearly_equal(panel_bounds.minimum.z, -6.0f));
    EXPECT_TRUE(nearly_equal(panel_bounds.maximum.z, 6.0f));

    auto minimum_radius{std::numeric_limits<float>::max()};
    for (auto const position : generate_honeycomb_panel({1, 1, 40.0f, 6.0f, 12.0f}).positions) {
        minimum_radius =
            std::min(minimum_radius, std::sqrt(position.x * position.x + position.y * position.y));
    }
    EXPECT_GT(minimum_radius, 0.0f);
}

TEST(GenerationRequests, ValidateDispatchDescribeAndAssignMaterialRoles) {
    auto request{make_default_request(Shape::hex_tile)};
    request.material_role = MaterialRole::armor;
    EXPECT_TRUE(validate_request(request).empty());
    auto const mesh{generate_mesh(request)};
    ASSERT_FALSE(mesh.triangle_material_roles.empty());
    EXPECT_TRUE(std::ranges::all_of(mesh.triangle_material_roles, [](MaterialRole const role) {
        return role == MaterialRole::armor;
    }));
    EXPECT_EQ(describe_request(request),
              "version=1;shape=hex_tile;outer_radius=50;depth=20;bevel_width=5;pointy_"
              "top=false;material_role=Armor");

    request.hex_tile.bevel_width = request.hex_tile.outer_radius;
    EXPECT_EQ(validate_request(request),
              "Hex-tile bevel width must be less than its radius and half-depth.");
}

TEST(AssemblyGeneration, AppendsOffsetsAndTransformsBuffers) {
    auto const box{generate_box()};
    MeshData assembly;
    append_transformed_mesh(assembly, box, {});
    append_transformed_mesh(assembly, box, {{100.0f, 0.0f, 0.0f}});
    ASSERT_EQ(assembly.positions.size(), 48);
    ASSERT_EQ(assembly.indices.size(), 72);
    EXPECT_EQ(assembly.indices[36], box.indices[0] + 24);
    EXPECT_TRUE(
        nearly_equal(assembly.positions[24],
                     {box.positions[0].x + 100.0f, box.positions[0].y, box.positions[0].z}));

    MeshData source;
    source.positions.push_back({1.0f, 0.0f, 0.0f});
    source.normals.push_back({0.70710678f, 0.70710678f, 0.0f});
    source.uvs.push_back({0.25f, 0.75f});
    source.indices.push_back(0);
    MeshData transformed;
    append_transformed_mesh(
        transformed, source, {{10.0f, 20.0f, 30.0f}, {0.0f, 90.0f, 0.0f}, {2.0f, 1.0f, 1.0f}});
    EXPECT_TRUE(nearly_equal(transformed.positions[0], {10.0f, 22.0f, 30.0f}));
    EXPECT_TRUE(nearly_equal(transformed.normals[0], {-0.894427f, 0.447214f, 0.0f}));
    EXPECT_EQ(transformed.uvs[0], source.uvs[0]);
}

TEST(AssemblyGeneration, ValidatesVisibilityAndDeterminism) {
    std::vector<AssemblyPart> parts{
        {make_default_request(Shape::box), {}},
        {make_default_request(Shape::cylinder),
         {{75.0f, 0.0f, 0.0f}, {0.0f, 45.0f, 0.0f}, {0.5f, 0.5f, 0.5f}}},
    };
    EXPECT_TRUE(validate_assembly(parts).empty());
    auto const first{generate_assembly(parts)};
    auto const second{generate_assembly(parts)};
    EXPECT_TRUE(same_mesh(first, second));
    EXPECT_EQ(describe_assembly(parts).find("version=1;assembly;part0={"), 0);

    parts[1].visible = false;
    EXPECT_EQ(generate_assembly(parts).positions.size(), generate_box().positions.size());
    parts[0].transform.scale.x = 0.0f;
    EXPECT_EQ(validate_assembly(parts), "Part 1: all scale values must be greater than zero.");
}

TEST(PrimitiveGenerators, AreDeterministic) {
    std::array<GenerationRequest, 9> requests{
        make_default_request(Shape::box),
        make_default_request(Shape::cylinder),
        make_default_request(Shape::sphere),
        make_default_request(Shape::cone),
        make_default_request(Shape::hex_tile),
        make_default_request(Shape::hex_frame),
        make_default_request(Shape::honeycomb_panel),
        make_default_request(Shape::beveled_box),
        make_default_request(Shape::wedge),
    };
    for (auto const& request : requests) {
        EXPECT_TRUE(same_mesh(generate_mesh(request), generate_mesh(request)));
    }
}

}
