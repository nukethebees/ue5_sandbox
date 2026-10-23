#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/vector.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mesh_gen {

using Vec2f = ioj::Vector<float, 2>;
using Vec3f = ioj::Vector<float, 3>;

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
    ioj::PositiveFinite<float> bevel_width{10.0f};
};

struct WedgeParameters {
    Vec3f dimensions{100.0f, 100.0f, 50.0f};
    ioj::PositiveFinite<float> top_length{50.0f};
    float top_offset{};
};

struct CylinderParameters {
    ioj::PositiveFinite<float> radius{50.0f};
    ioj::PositiveFinite<float> height{100.0f};
    std::int32_t radial_segments{32};
};

struct ConeParameters {
    ioj::PositiveFinite<float> radius{50.0f};
    ioj::PositiveFinite<float> height{100.0f};
    std::int32_t radial_segments{32};
};

struct SphereParameters {
    ioj::PositiveFinite<float> radius{50.0f};
    std::int32_t longitude_segments{32};
    std::int32_t latitude_segments{16};
};

struct HexTileParameters {
    ioj::PositiveFinite<float> outer_radius{50.0f};
    ioj::PositiveFinite<float> depth{20.0f};
    ioj::PositiveFinite<float> bevel_width{5.0f};
    bool pointy_top{};
};

enum class HexFrameUvMode : std::uint8_t {
    per_face,
    perimeter,
};

struct HexFrameParameters {
    ioj::PositiveFinite<float> outer_radius{50.0f};
    ioj::PositiveFinite<float> wall_thickness{10.0f};
    ioj::PositiveFinite<float> depth{20.0f};
    bool pointy_top{};
    HexFrameUvMode uv_mode{HexFrameUvMode::per_face};
};

struct HoneycombPanelParameters {
    ioj::Positive<std::int32_t> rows{3};
    ioj::Positive<std::int32_t> columns{4};
    ioj::PositiveFinite<float> cell_radius{50.0f};
    ioj::PositiveFinite<float> wall_thickness{8.0f};
    ioj::PositiveFinite<float> depth{20.0f};
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

// Require positive dimensions and the shape-specific relationships checked by validate_request.
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
// Require validate_request(request) to succeed before generating an authored request.
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
