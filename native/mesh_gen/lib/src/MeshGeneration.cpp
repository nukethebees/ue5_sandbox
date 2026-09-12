#include <mesh_gen/MeshGeneration.h>

#include "MeshMath.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <sstream>
#include <string>

namespace mesh_gen {
namespace {

using namespace math;

[[nodiscard]] auto role_name(MaterialRole const role) -> char const* {
    switch (role) {
        case MaterialRole::structure:
            return "Structure";
        case MaterialRole::armor:
            return "Armor";
        case MaterialRole::glass:
            return "Glass";
        case MaterialRole::emissive:
            return "Emissive";
    }
    return "";
}

[[nodiscard]] auto format_number(float const value) -> std::string {
    char buffer[64]{};
    auto const result{std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value))};
    if (result < 0) {
        return {};
    }
    return buffer;
}

[[nodiscard]] auto format_vec(Vec3f const value) -> std::string {
    char buffer[160]{};
    auto const result{std::snprintf(buffer,
                                    sizeof(buffer),
                                    "X=%.3f Y=%.3f Z=%.3f",
                                    static_cast<double>(value.x),
                                    static_cast<double>(value.y),
                                    static_cast<double>(value.z))};
    if (result < 0) {
        return {};
    }
    return buffer;
}

[[nodiscard]] auto format_rotator(Rotatorf const value) -> std::string {
    char buffer[160]{};
    auto const result{std::snprintf(buffer,
                                    sizeof(buffer),
                                    "P=%.3f Y=%.3f R=%.3f",
                                    static_cast<double>(value.pitch),
                                    static_cast<double>(value.yaw),
                                    static_cast<double>(value.roll))};
    if (result < 0) {
        return {};
    }
    return buffer;
}

}

namespace math {

auto rotate(Vec3f const value, Rotatorf const rotation) -> Vec3f {
    constexpr auto degrees_to_half_radians{std::numbers::pi_v<float> / 360.0f};
    auto const pitch{rotation.pitch * degrees_to_half_radians};
    auto const yaw{rotation.yaw * degrees_to_half_radians};
    auto const roll{rotation.roll * degrees_to_half_radians};
    auto const sp{std::sin(pitch)};
    auto const cp{std::cos(pitch)};
    auto const sy{std::sin(yaw)};
    auto const cy{std::cos(yaw)};
    auto const sr{std::sin(roll)};
    auto const cr{std::cos(roll)};

    auto const quaternion_x{cr * sp * sy - sr * cp * cy};
    auto const quaternion_y{-cr * sp * cy - sr * cp * sy};
    auto const quaternion_z{cr * cp * sy - sr * sp * cy};
    auto const quaternion_w{cr * cp * cy + sr * sp * sy};
    Vec3f const quaternion_vector{quaternion_x, quaternion_y, quaternion_z};
    auto const twice_cross{cross(quaternion_vector, value) * 2.0f};
    return value + twice_cross * quaternion_w + cross(quaternion_vector, twice_cross);
}

}

auto make_default_request(Shape const shape) -> GenerationRequest {
    GenerationRequest request;
    request.shape = shape;
    switch (shape) {
        case Shape::box:
            request.asset_name = "SM_GeneratedBox";
            break;
        case Shape::cylinder:
            request.asset_name = "SM_GeneratedCylinder";
            break;
        case Shape::sphere:
            request.asset_name = "SM_GeneratedSphere";
            break;
        case Shape::cone:
            request.asset_name = "SM_GeneratedCone";
            break;
        case Shape::hex_tile:
            request.asset_name = "SM_GeneratedHexTile";
            break;
        case Shape::hex_frame:
            request.asset_name = "SM_GeneratedHexFrame";
            break;
        case Shape::honeycomb_panel:
            request.asset_name = "SM_GeneratedHoneycombPanel";
            break;
        case Shape::beveled_box:
            request.asset_name = "SM_GeneratedBeveledBox";
            break;
        case Shape::wedge:
            request.asset_name = "SM_GeneratedWedge";
            break;
    }
    return request;
}

auto validate_request(GenerationRequest const& request) -> std::string {
    if (request.asset_name.empty()) {
        return "Asset name must not be empty.";
    }

    switch (request.shape) {
        case Shape::box:
            if (minimum_component(request.box.dimensions) <= 0.0f) {
                return "All box dimensions must be greater than zero.";
            }
            break;
        case Shape::cylinder:
            if (request.cylinder.radius <= 0.0f || request.cylinder.height <= 0.0f ||
                request.cylinder.radial_segments < 3) {
                return "Cylinder radius and height must be positive, with at least 3 segments.";
            }
            break;
        case Shape::sphere:
            if (request.sphere.radius <= 0.0f || request.sphere.longitude_segments < 3 ||
                request.sphere.latitude_segments < 2) {
                return "Sphere radius must be positive, with at least 3 longitude and 2 latitude "
                       "segments.";
            }
            break;
        case Shape::cone:
            if (request.cone.radius <= 0.0f || request.cone.height <= 0.0f ||
                request.cone.radial_segments < 3) {
                return "Cone radius and height must be positive, with at least 3 segments.";
            }
            break;
        case Shape::hex_tile:
            if (request.hex_tile.outer_radius <= 0.0f || request.hex_tile.depth <= 0.0f ||
                request.hex_tile.bevel_width <= 0.0f) {
                return "Hex-tile radius, depth, and bevel width must be positive.";
            }
            if (request.hex_tile.bevel_width >= request.hex_tile.outer_radius ||
                request.hex_tile.bevel_width >= request.hex_tile.depth * 0.5f) {
                return "Hex-tile bevel width must be less than its radius and half-depth.";
            }
            break;
        case Shape::hex_frame:
            if (request.hex_frame.outer_radius <= 0.0f ||
                request.hex_frame.wall_thickness <= 0.0f || request.hex_frame.depth <= 0.0f) {
                return "Hex-frame radius, wall thickness, and depth must be positive.";
            }
            if (request.hex_frame.wall_thickness >= request.hex_frame.outer_radius) {
                return "Hex-frame wall thickness must be less than its outer radius.";
            }
            if (request.hex_frame.uv_mode < HexFrameUvMode::per_face ||
                request.hex_frame.uv_mode > HexFrameUvMode::perimeter) {
                return "Hex-frame UV mode is invalid.";
            }
            break;
        case Shape::honeycomb_panel:
            if (request.honeycomb_panel.rows < 1 || request.honeycomb_panel.columns < 1 ||
                request.honeycomb_panel.cell_radius <= 0.0f ||
                request.honeycomb_panel.wall_thickness <= 0.0f ||
                request.honeycomb_panel.depth <= 0.0f) {
                return "Honeycomb rows and columns must be at least 1; radius, wall thickness, "
                       "and depth must be positive.";
            }
            if (request.honeycomb_panel.wall_thickness >= request.honeycomb_panel.cell_radius) {
                return "Honeycomb wall thickness must be less than its cell radius.";
            }
            break;
        case Shape::beveled_box:
            if (minimum_component(request.beveled_box.dimensions) <= 0.0f ||
                request.beveled_box.bevel_width <= 0.0f) {
                return "Beveled-box dimensions and bevel width must be positive.";
            }
            if (request.beveled_box.bevel_width >=
                minimum_component(request.beveled_box.dimensions) * 0.5f) {
                return "Beveled-box bevel width must be less than half its smallest dimension.";
            }
            break;
        case Shape::wedge:
            if (minimum_component(request.wedge.dimensions) <= 0.0f ||
                request.wedge.top_length <= 0.0f) {
                return "Wedge dimensions and top length must be positive.";
            }
            if (request.wedge.top_length > request.wedge.dimensions.x ||
                std::abs(request.wedge.top_offset) + request.wedge.top_length * 0.5f >
                    request.wedge.dimensions.x * 0.5f) {
                return "Wedge top length and offset must keep the top within its base.";
            }
            break;
    }
    return {};
}

auto generate_mesh(GenerationRequest const& request) -> MeshData {
    assert(validate_request(request).empty());
    MeshData mesh_data;
    switch (request.shape) {
        case Shape::box:
            mesh_data = generate_box(request.box);
            break;
        case Shape::cylinder:
            mesh_data = generate_cylinder(request.cylinder);
            break;
        case Shape::sphere:
            mesh_data = generate_sphere(request.sphere);
            break;
        case Shape::cone:
            mesh_data = generate_cone(request.cone);
            break;
        case Shape::hex_tile:
            mesh_data = generate_hex_tile(request.hex_tile);
            break;
        case Shape::hex_frame:
            mesh_data = generate_hex_frame(request.hex_frame);
            break;
        case Shape::honeycomb_panel:
            mesh_data = generate_honeycomb_panel(request.honeycomb_panel);
            break;
        case Shape::beveled_box:
            mesh_data = generate_beveled_box(request.beveled_box);
            break;
        case Shape::wedge:
            mesh_data = generate_wedge(request.wedge);
            break;
    }
    mesh_data.triangle_material_roles.assign(mesh_data.indices.size() / 3, request.material_role);
    return mesh_data;
}

auto describe_request(GenerationRequest const& request) -> std::string {
    std::ostringstream description;
    description << "version=1;shape=";
    switch (request.shape) {
        case Shape::box:
            description << "box;dimensions=" << format_vec(request.box.dimensions);
            break;
        case Shape::cylinder:
            description << "cylinder;radius=" << format_number(request.cylinder.radius)
                        << ";height=" << format_number(request.cylinder.height)
                        << ";segments=" << request.cylinder.radial_segments;
            break;
        case Shape::sphere:
            description << "sphere;radius=" << format_number(request.sphere.radius)
                        << ";longitude_segments=" << request.sphere.longitude_segments
                        << ";latitude_segments=" << request.sphere.latitude_segments;
            break;
        case Shape::cone:
            description << "cone;radius=" << format_number(request.cone.radius)
                        << ";height=" << format_number(request.cone.height)
                        << ";segments=" << request.cone.radial_segments;
            break;
        case Shape::hex_tile:
            description << "hex_tile;outer_radius=" << format_number(request.hex_tile.outer_radius)
                        << ";depth=" << format_number(request.hex_tile.depth)
                        << ";bevel_width=" << format_number(request.hex_tile.bevel_width)
                        << ";pointy_top=" << (request.hex_tile.pointy_top ? "true" : "false");
            break;
        case Shape::hex_frame:
            description << "hex_frame;outer_radius="
                        << format_number(request.hex_frame.outer_radius)
                        << ";wall_thickness=" << format_number(request.hex_frame.wall_thickness)
                        << ";depth=" << format_number(request.hex_frame.depth)
                        << ";pointy_top=" << (request.hex_frame.pointy_top ? "true" : "false")
                        << ";uv_mode="
                        << (request.hex_frame.uv_mode == HexFrameUvMode::perimeter ? "perimeter"
                                                                                   : "per_face");
            break;
        case Shape::honeycomb_panel:
            description << "honeycomb_panel;rows=" << request.honeycomb_panel.rows
                        << ";columns=" << request.honeycomb_panel.columns
                        << ";cell_radius=" << format_number(request.honeycomb_panel.cell_radius)
                        << ";wall_thickness="
                        << format_number(request.honeycomb_panel.wall_thickness)
                        << ";depth=" << format_number(request.honeycomb_panel.depth)
                        << ";pointy_top="
                        << (request.honeycomb_panel.pointy_top ? "true" : "false");
            break;
        case Shape::beveled_box:
            description << "beveled_box;dimensions=" << format_vec(request.beveled_box.dimensions)
                        << ";bevel_width=" << format_number(request.beveled_box.bevel_width);
            break;
        case Shape::wedge:
            description << "wedge;dimensions=" << format_vec(request.wedge.dimensions)
                        << ";top_length=" << format_number(request.wedge.top_length)
                        << ";top_offset=" << format_number(request.wedge.top_offset);
            break;
    }
    description << ";material_role=" << role_name(request.material_role);
    return description.str();
}

auto is_valid_mesh_data(MeshData const& mesh_data) -> bool {
    if (mesh_data.positions.empty() || mesh_data.indices.empty() ||
        mesh_data.indices.size() % 3 != 0 ||
        mesh_data.normals.size() != mesh_data.positions.size() ||
        mesh_data.uvs.size() != mesh_data.positions.size() ||
        (!mesh_data.triangle_material_roles.empty() &&
         mesh_data.triangle_material_roles.size() != mesh_data.indices.size() / 3)) {
        return false;
    }
    for (auto const index : mesh_data.indices) {
        if (index >= mesh_data.positions.size()) {
            return false;
        }
    }
    return true;
}

void append_transformed_mesh(MeshData& destination,
                             MeshData const& source,
                             Transform const& transform) {
    auto const vertex_offset{static_cast<std::uint32_t>(destination.positions.size())};
    destination.positions.reserve(destination.positions.size() + source.positions.size());
    destination.normals.reserve(destination.normals.size() + source.normals.size());
    destination.uvs.reserve(destination.uvs.size() + source.uvs.size());
    destination.indices.reserve(destination.indices.size() + source.indices.size());
    auto const source_triangle_count{source.indices.size() / 3};
    destination.triangle_material_roles.reserve(destination.triangle_material_roles.size() +
                                                source_triangle_count);

    for (auto const position : source.positions) {
        destination.positions.push_back(
            rotate(component_multiply(position, transform.scale), transform.rotation) +
            transform.translation);
    }
    for (auto const normal : source.normals) {
        auto const inverse_scaled_normal{Vec3f{normal.x / transform.scale.x,
                                               normal.y / transform.scale.y,
                                               normal.z / transform.scale.z}};
        destination.normals.push_back(normalize(rotate(inverse_scaled_normal, transform.rotation)));
    }
    destination.uvs.insert(destination.uvs.end(), source.uvs.begin(), source.uvs.end());
    for (auto const index : source.indices) {
        destination.indices.push_back(vertex_offset + index);
    }
    if (source.triangle_material_roles.empty()) {
        destination.triangle_material_roles.insert(destination.triangle_material_roles.end(),
                                                   source_triangle_count,
                                                   MaterialRole::structure);
    } else {
        assert(source.triangle_material_roles.size() == source_triangle_count);
        destination.triangle_material_roles.insert(destination.triangle_material_roles.end(),
                                                   source.triangle_material_roles.begin(),
                                                   source.triangle_material_roles.end());
    }
}

auto validate_assembly(std::span<AssemblyPart const> const parts) -> std::string {
    if (parts.empty()) {
        return "Add at least one part to the assembly.";
    }
    for (std::size_t part_index{}; part_index < parts.size(); ++part_index) {
        auto const& part{parts[part_index]};
        auto const mesh_error{validate_request(part.mesh)};
        if (!mesh_error.empty()) {
            return "Part " + std::to_string(part_index + 1) + ": " + mesh_error;
        }
        if (minimum_component(part.transform.scale) <= 0.0f) {
            return "Part " + std::to_string(part_index + 1) +
                   ": all scale values must be greater than zero.";
        }
    }
    return {};
}

auto generate_assembly(std::span<AssemblyPart const> const parts) -> MeshData {
    assert(validate_assembly(parts).empty());
    MeshData assembly;
    for (auto const& part : parts) {
        if (part.visible) {
            append_transformed_mesh(assembly, generate_mesh(part.mesh), part.transform);
        }
    }
    return assembly;
}

auto describe_assembly(std::span<AssemblyPart const> const parts) -> std::string {
    std::ostringstream description;
    description << "version=1;assembly";
    for (std::size_t part_index{}; part_index < parts.size(); ++part_index) {
        auto const& part{parts[part_index]};
        description << ";part" << part_index << "={" << describe_request(part.mesh)
                    << ";translation=" << format_vec(part.transform.translation)
                    << ";rotation=" << format_rotator(part.transform.rotation)
                    << ";scale=" << format_vec(part.transform.scale)
                    << ";visible=" << (part.visible ? "true" : "false") << '}';
    }
    return description.str();
}

}
