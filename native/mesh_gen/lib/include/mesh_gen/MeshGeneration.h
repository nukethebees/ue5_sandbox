#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mesh_gen {

struct Vec2f {
    float x{};
    float y{};

    [[nodiscard]] auto operator==(Vec2f const&) const -> bool = default;
};

struct Vec3f {
    float x{};
    float y{};
    float z{};

    [[nodiscard]] auto operator==(Vec3f const&) const -> bool = default;
};

struct Rotatorf {
    float pitch{};
    float yaw{};
    float roll{};

    [[nodiscard]] auto operator==(Rotatorf const&) const -> bool = default;
};

enum class MaterialRole : std::uint8_t {
    structure,
    armor,
    glass,
    emissive,
};

enum class Shape : std::uint8_t {
    box,
    cylinder,
    sphere,
    cone,
    hex_tile,
    hex_frame,
    honeycomb_panel,
    beveled_box,
    wedge,
};

struct MeshData {
    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;
    std::vector<Vec2f> uvs;
    std::vector<std::uint32_t> indices;
    std::vector<MaterialRole> triangle_material_roles;
};

struct BoxParameters {
    Vec3f dimensions{100.0f, 100.0f, 100.0f};
};

struct BeveledBoxParameters {
    Vec3f dimensions{100.0f, 100.0f, 100.0f};
    float bevel_width{10.0f};
};

struct WedgeParameters {
    Vec3f dimensions{100.0f, 100.0f, 50.0f};
    float top_length{50.0f};
    float top_offset{};
};

struct CylinderParameters {
    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};

struct ConeParameters {
    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};

struct SphereParameters {
    float radius{50.0f};
    std::int32_t longitude_segments{32};
    std::int32_t latitude_segments{16};
};

struct HexTileParameters {
    float outer_radius{50.0f};
    float depth{20.0f};
    float bevel_width{5.0f};
    bool pointy_top{};
};

struct HexFrameParameters {
    float outer_radius{50.0f};
    float wall_thickness{10.0f};
    float depth{20.0f};
    bool pointy_top{};
};

struct HoneycombPanelParameters {
    std::int32_t rows{3};
    std::int32_t columns{4};
    float cell_radius{50.0f};
    float wall_thickness{8.0f};
    float depth{20.0f};
    bool pointy_top{};
};

struct GenerationRequest {
    Shape shape{Shape::box};
    std::string asset_name{"SM_GeneratedBox"};
    MaterialRole material_role{MaterialRole::structure};
    BoxParameters box;
    CylinderParameters cylinder;
    SphereParameters sphere;
    ConeParameters cone;
    HexTileParameters hex_tile;
    HexFrameParameters hex_frame;
    HoneycombPanelParameters honeycomb_panel;
    BeveledBoxParameters beveled_box;
    WedgeParameters wedge;
};

struct Transform {
    Vec3f translation{};
    Rotatorf rotation{};
    Vec3f scale{1.0f, 1.0f, 1.0f};
};

struct AssemblyPart {
    GenerationRequest mesh;
    Transform transform;
    bool visible{true};
    bool locked{};
};

[[nodiscard]] auto generate_box(BoxParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_beveled_box(BeveledBoxParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_wedge(WedgeParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_cylinder(CylinderParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_cone(ConeParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_sphere(SphereParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_hex_tile(HexTileParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_hex_frame(HexFrameParameters const& parameters = {}) -> MeshData;
[[nodiscard]] auto generate_honeycomb_panel(HoneycombPanelParameters const& parameters = {})
    -> MeshData;

[[nodiscard]] auto make_default_request(Shape shape) -> GenerationRequest;
[[nodiscard]] auto validate_request(GenerationRequest const& request) -> std::string;
[[nodiscard]] auto generate_mesh(GenerationRequest const& request) -> MeshData;
[[nodiscard]] auto describe_request(GenerationRequest const& request) -> std::string;

[[nodiscard]] auto is_valid_mesh_data(MeshData const& mesh_data) -> bool;
void append_transformed_mesh(MeshData& destination,
                             MeshData const& source,
                             Transform const& transform);
[[nodiscard]] auto validate_assembly(std::span<AssemblyPart const> parts) -> std::string;
[[nodiscard]] auto generate_assembly(std::span<AssemblyPart const> parts) -> MeshData;
[[nodiscard]] auto describe_assembly(std::span<AssemblyPart const> parts) -> std::string;

}
