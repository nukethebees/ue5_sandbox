#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>
#include <ioj/vector.h>

#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
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

struct ValidParameters {
    [[nodiscard]] constexpr auto operator()(auto const& input) const -> bool {
        return input.error().empty();
    }
};

inline constexpr auto parameters_policy{ioj::ConstrainedValuePolicy{
    ValidParameters{}, "Mesh parameters violate their shape constraints"}};

// Keep the policy value out of native/Unreal function signatures because their mangling differs.
template <typename Input>
class CheckedParameters : public ioj::ConstrainedValue<Input, parameters_policy> {
    using Base = ioj::ConstrainedValue<Input, parameters_policy>;

  public:
    using Base::Base;

    constexpr CheckedParameters() = default;

    [[nodiscard]] static auto
        make(Input const& input,
             std::source_location const location = std::source_location::current())
            -> CheckedParameters {
        return CheckedParameters{Base::make(input, location)};
    }

    [[nodiscard]] static constexpr auto try_make(Input const& input)
        -> std::optional<CheckedParameters> {
        auto const checked{Base::try_make(input)};
        if (!checked) {
            return std::nullopt;
        }
        return CheckedParameters{*checked};
    }

  private:
    explicit constexpr CheckedParameters(Base const& checked)
        : Base{checked} {}
};

struct BoxInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        return ioj::PositiveFiniteVector<float, 3>::valid(dimensions)
                 ? ""
                 : "All box dimensions must be finite and greater than zero.";
    }

    Vec3f dimensions{100.0f, 100.0f, 100.0f};
};
using BoxParameters = CheckedParameters<BoxInput>;

struct BeveledBoxInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFiniteVector<float, 3>::valid(dimensions) ||
            !ioj::PositiveFinite<float>::valid(bevel_width)) {
            return "Beveled-box dimensions and bevel width must be finite and positive.";
        }
        for (auto const dimension : dimensions.elements) {
            if (bevel_width >= dimension * 0.5f) {
                return "Beveled-box bevel width must be less than half its smallest dimension.";
            }
        }
        return {};
    }

    Vec3f dimensions{100.0f, 100.0f, 100.0f};
    float bevel_width{10.0f};
};
using BeveledBoxParameters = CheckedParameters<BeveledBoxInput>;

struct WedgeInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFiniteVector<float, 3>::valid(dimensions) ||
            !ioj::PositiveFinite<float>::valid(top_length)) {
            return "Wedge dimensions and top length must be finite and positive.";
        }
        if (!ioj::Finite<float>::valid(top_offset)) {
            return "Wedge top offset must be finite.";
        }
        auto const offset{top_offset < 0.f ? -top_offset : top_offset};
        if (top_length > dimensions.x() || offset + top_length * 0.5f > dimensions.x() * 0.5f) {
            return "Wedge top length and offset must keep the top within its base.";
        }
        return {};
    }

    Vec3f dimensions{100.0f, 100.0f, 50.0f};
    float top_length{50.0f};
    float top_offset{};
};
using WedgeParameters = CheckedParameters<WedgeInput>;

struct CylinderInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius) ||
            !ioj::PositiveFinite<float>::valid(height)) {
            return "Cylinder radius and height must be finite and positive.";
        }
        return radial_segments >= 3
                 ? ""
                 : "Cylinder radius and height must be positive, with at least 3 segments.";
    }

    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};
using CylinderParameters = CheckedParameters<CylinderInput>;

struct ConeInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius) ||
            !ioj::PositiveFinite<float>::valid(height)) {
            return "Cone radius and height must be finite and positive.";
        }
        return radial_segments >= 3
                 ? ""
                 : "Cone radius and height must be positive, with at least 3 segments.";
    }

    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};
using ConeParameters = CheckedParameters<ConeInput>;

struct SphereInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius)) {
            return "Sphere radius must be finite and positive.";
        }
        return longitude_segments >= 3 && latitude_segments >= 2
                 ? ""
                 : "Sphere radius must be positive, with at least 3 longitude and 2 latitude "
                   "segments.";
    }

    float radius{50.0f};
    std::int32_t longitude_segments{32};
    std::int32_t latitude_segments{16};
};
using SphereParameters = CheckedParameters<SphereInput>;

struct HexTileInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(outer_radius) ||
            !ioj::PositiveFinite<float>::valid(depth) ||
            !ioj::PositiveFinite<float>::valid(bevel_width)) {
            return "Hex-tile radius, depth, and bevel width must be finite and positive.";
        }
        return bevel_width < outer_radius && bevel_width < depth * 0.5f
                 ? ""
                 : "Hex-tile bevel width must be less than its radius and half-depth.";
    }

    float outer_radius{50.0f};
    float depth{20.0f};
    float bevel_width{5.0f};
    bool pointy_top{};
};
using HexTileParameters = CheckedParameters<HexTileInput>;

enum class HexFrameUvMode : std::uint8_t {
    per_face,
    perimeter,
};

struct HexFrameInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(outer_radius) ||
            !ioj::PositiveFinite<float>::valid(wall_thickness) ||
            !ioj::PositiveFinite<float>::valid(depth)) {
            return "Hex-frame radius, wall thickness, and depth must be finite and positive.";
        }
        if (wall_thickness >= outer_radius) {
            return "Hex-frame wall thickness must be less than its outer radius.";
        }
        return uv_mode == HexFrameUvMode::per_face || uv_mode == HexFrameUvMode::perimeter
                 ? ""
                 : "Hex-frame UV mode is invalid.";
    }

    float outer_radius{50.0f};
    float wall_thickness{10.0f};
    float depth{20.0f};
    bool pointy_top{};
    HexFrameUvMode uv_mode{HexFrameUvMode::per_face};
};
using HexFrameParameters = CheckedParameters<HexFrameInput>;

struct HoneycombPanelInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (rows <= 0 || columns <= 0 || !ioj::PositiveFinite<float>::valid(cell_radius) ||
            !ioj::PositiveFinite<float>::valid(wall_thickness) ||
            !ioj::PositiveFinite<float>::valid(depth)) {
            return "Honeycomb rows and columns must be positive; radius, wall thickness, and depth "
                   "must be finite and positive.";
        }
        return wall_thickness < cell_radius
                 ? ""
                 : "Honeycomb wall thickness must be less than its cell radius.";
    }

    std::int32_t rows{3};
    std::int32_t columns{4};
    float cell_radius{50.0f};
    float wall_thickness{8.0f};
    float depth{20.0f};
    bool pointy_top{};
};
using HoneycombPanelParameters = CheckedParameters<HoneycombPanelInput>;

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

struct TransformInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        return ioj::PositiveFiniteVector<float, 3>::valid(scale)
                 ? ""
                 : "all scale values must be finite and greater than zero.";
    }

    Vec3f translation{};
    Rotatorf rotation{};
    Vec3f scale{1.0f, 1.0f, 1.0f};
};
using Transform = CheckedParameters<TransformInput>;

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
[[nodiscard]] auto generate_mesh(GenerationRequest const& request) -> MeshData;
[[nodiscard]] auto describe_request(GenerationRequest const& request) -> std::string;

[[nodiscard]] auto is_valid_mesh_data(MeshData const& mesh_data) -> bool;
void append_transformed_mesh(MeshData& destination,
                             MeshData const& source,
                             Transform const& transform);
[[nodiscard]] auto generate_assembly(std::span<AssemblyPart const> parts) -> MeshData;
[[nodiscard]] auto describe_assembly(std::span<AssemblyPart const> parts) -> std::string;

}
