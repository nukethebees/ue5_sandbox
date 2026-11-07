#include "mesh_math.h"
#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>
#include <ioj/format_writer.h>

#include <mesh_gen/assembly_part.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <numbers>
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
                                    static_cast<double>(value.x()),
                                    static_cast<double>(value.y()),
                                    static_cast<double>(value.z()))};
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

auto validate_request(GenerationRequest const& request) -> std::string_view {
    if (request.asset_name.empty()) {
        return "Asset name must not be empty.";
    }
    switch (request.shape) {
        case Shape::box: {
            auto const& parameters{request.box};
            return ioj::PositiveFiniteVector<float, 3>::valid(parameters.dimensions)
                     ? ""
                     : "All box dimensions must be finite and greater than zero.";
        }
        case Shape::beveled_box: {
            auto const& parameters{request.beveled_box};
            if (!ioj::PositiveFiniteVector<float, 3>::valid(parameters.dimensions) ||
                !ioj::PositiveFinite<float>::valid(parameters.bevel_width)) {
                return "Beveled-box dimensions and bevel width must be finite and positive.";
            }
            for (auto const dimension : parameters.dimensions.elements) {
                if (parameters.bevel_width >= dimension * 0.5f) {
                    return "Beveled-box bevel width must be less than half its smallest dimension.";
                }
            }
            return {};
        }
        case Shape::wedge: {
            auto const& parameters{request.wedge};
            if (!ioj::PositiveFiniteVector<float, 3>::valid(parameters.dimensions) ||
                !ioj::PositiveFinite<float>::valid(parameters.top_length)) {
                return "Wedge dimensions and top length must be finite and positive.";
            }
            if (!ioj::Finite<float>::valid(parameters.top_offset)) {
                return "Wedge top offset must be finite.";
            }
            auto const offset{parameters.top_offset < 0.f ? -parameters.top_offset
                                                          : parameters.top_offset};
            if (parameters.top_length > parameters.dimensions.x() ||
                offset + parameters.top_length * 0.5f > parameters.dimensions.x() * 0.5f) {
                return "Wedge top length and offset must keep the top within its base.";
            }
            return {};
        }
        case Shape::cylinder: {
            auto const& parameters{request.cylinder};
            if (!ioj::PositiveFinite<float>::valid(parameters.radius) ||
                !ioj::PositiveFinite<float>::valid(parameters.height)) {
                return "Cylinder radius and height must be finite and positive.";
            }
            return parameters.radial_segments >= 3
                     ? ""
                     : "Cylinder radius and height must be positive, with at least 3 segments.";
        }
        case Shape::cone: {
            auto const& parameters{request.cone};
            if (!ioj::PositiveFinite<float>::valid(parameters.radius) ||
                !ioj::PositiveFinite<float>::valid(parameters.height)) {
                return "Cone radius and height must be finite and positive.";
            }
            return parameters.radial_segments >= 3
                     ? ""
                     : "Cone radius and height must be positive, with at least 3 segments.";
        }
        case Shape::sphere: {
            auto const& parameters{request.sphere};
            if (!ioj::PositiveFinite<float>::valid(parameters.radius)) {
                return "Sphere radius must be finite and positive.";
            }
            return parameters.longitude_segments >= 3 && parameters.latitude_segments >= 2
                     ? ""
                     : "Sphere radius must be positive, with at least 3 longitude and 2 latitude "
                       "segments.";
        }
        case Shape::hex_tile: {
            auto const& parameters{request.hex_tile};
            if (!ioj::PositiveFinite<float>::valid(parameters.outer_radius) ||
                !ioj::PositiveFinite<float>::valid(parameters.depth) ||
                !ioj::PositiveFinite<float>::valid(parameters.bevel_width)) {
                return "Hex-tile radius, depth, and bevel width must be finite and positive.";
            }
            return parameters.bevel_width < parameters.outer_radius &&
                           parameters.bevel_width < parameters.depth * 0.5f
                     ? ""
                     : "Hex-tile bevel width must be less than its radius and half-depth.";
        }
        case Shape::hex_frame: {
            auto const& parameters{request.hex_frame};
            if (!ioj::PositiveFinite<float>::valid(parameters.outer_radius) ||
                !ioj::PositiveFinite<float>::valid(parameters.wall_thickness) ||
                !ioj::PositiveFinite<float>::valid(parameters.depth)) {
                return "Hex-frame radius, wall thickness, and depth must be finite and positive.";
            }
            if (parameters.wall_thickness >= parameters.outer_radius) {
                return "Hex-frame wall thickness must be less than its outer radius.";
            }
            return parameters.uv_mode == HexFrameUvMode::per_face ||
                           parameters.uv_mode == HexFrameUvMode::perimeter
                     ? ""
                     : "Hex-frame UV mode is invalid.";
        }
        case Shape::honeycomb_panel: {
            auto const& parameters{request.honeycomb_panel};
            if (parameters.rows <= 0 || parameters.columns <= 0 ||
                !ioj::PositiveFinite<float>::valid(parameters.cell_radius) ||
                !ioj::PositiveFinite<float>::valid(parameters.wall_thickness) ||
                !ioj::PositiveFinite<float>::valid(parameters.depth)) {
                return "Honeycomb rows and columns must be positive; radius, wall thickness, and "
                       "depth "
                       "must be finite and positive.";
            }
            return parameters.wall_thickness < parameters.cell_radius
                     ? ""
                     : "Honeycomb wall thickness must be less than its cell radius.";
        }
    }
    return "Mesh shape is invalid.";
}

auto validate_transform(Transform const& transform) -> std::string_view {
    if (!ioj::FiniteVector<float, 3>::valid(transform.translation) ||
        !ioj::Finite<float>::valid(transform.rotation.pitch) ||
        !ioj::Finite<float>::valid(transform.rotation.yaw) ||
        !ioj::Finite<float>::valid(transform.rotation.roll)) {
        return "Transform translation and rotation must be finite.";
    }
    return ioj::PositiveFiniteVector<float, 3>::valid(transform.scale)
             ? ""
             : "Transform scale values must be finite and greater than zero.";
}

auto generate_mesh(GenerationRequest const& request) -> MeshData {
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
    std::string description{"version=1;shape="};
    auto write{ioj::FormatWriter{std::back_inserter(description)}};
    switch (request.shape) {
        case Shape::box:
            write("box;dimensions={}", format_vec(request.box.dimensions));
            break;
        case Shape::cylinder: {
            auto const& parameters{request.cylinder};
            write("cylinder;radius={};height={};segments={}",
                  format_number(parameters.radius),
                  format_number(parameters.height),
                  parameters.radial_segments);
            break;
        }
        case Shape::sphere: {
            auto const& parameters{request.sphere};
            write("sphere;radius={};longitude_segments={};latitude_segments={}",
                  format_number(parameters.radius),
                  parameters.longitude_segments,
                  parameters.latitude_segments);
            break;
        }
        case Shape::cone: {
            auto const& parameters{request.cone};
            write("cone;radius={};height={};segments={}",
                  format_number(parameters.radius),
                  format_number(parameters.height),
                  parameters.radial_segments);
            break;
        }
        case Shape::hex_tile: {
            auto const& parameters{request.hex_tile};
            write("hex_tile;outer_radius={};depth={};bevel_width={};pointy_top={}",
                  format_number(parameters.outer_radius),
                  format_number(parameters.depth),
                  format_number(parameters.bevel_width),
                  parameters.pointy_top);
            break;
        }
        case Shape::hex_frame: {
            auto const& parameters{request.hex_frame};
            write("hex_frame;outer_radius={};wall_thickness={};depth={};pointy_top={};uv_mode={}",
                  format_number(parameters.outer_radius),
                  format_number(parameters.wall_thickness),
                  format_number(parameters.depth),
                  parameters.pointy_top,
                  parameters.uv_mode == HexFrameUvMode::perimeter ? "perimeter" : "per_face");
            break;
        }
        case Shape::honeycomb_panel: {
            auto const& parameters{request.honeycomb_panel};
            write("honeycomb_panel;rows={};columns={};cell_radius={};wall_thickness={};"
                  "depth={};pointy_top={}",
                  parameters.rows,
                  parameters.columns,
                  format_number(parameters.cell_radius),
                  format_number(parameters.wall_thickness),
                  format_number(parameters.depth),
                  parameters.pointy_top);
            break;
        }
        case Shape::beveled_box: {
            auto const& parameters{request.beveled_box};
            write("beveled_box;dimensions={};bevel_width={}",
                  format_vec(parameters.dimensions),
                  format_number(parameters.bevel_width));
            break;
        }
        case Shape::wedge: {
            auto const& parameters{request.wedge};
            write("wedge;dimensions={};top_length={};top_offset={}",
                  format_vec(parameters.dimensions),
                  format_number(parameters.top_length),
                  format_number(parameters.top_offset));
            break;
        }
    }
    write(";material_role={}", role_name(request.material_role));
    return description;
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
        auto const inverse_scaled_normal{Vec3f{normal.x() / transform.scale.x(),
                                               normal.y() / transform.scale.y(),
                                               normal.z() / transform.scale.z()}};
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
    auto const count{parts.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& part{parts[index]};
        auto error{validate_request(part.mesh)};
        if (error.empty()) {
            error = validate_transform(part.transform);
        }
        if (!error.empty()) {
            return "Part " + std::to_string(index + 1) + ": " + std::string{error};
        }
    }
    return {};
}

auto generate_assembly(std::span<AssemblyPart const> const parts) -> MeshData {
    MeshData assembly;
    for (auto const& part : parts) {
        if (part.visible) {
            append_transformed_mesh(assembly, generate_mesh(part.mesh), part.transform);
        }
    }
    return assembly;
}

auto describe_assembly(std::span<AssemblyPart const> const parts) -> std::string {
    std::string description{"version=1;assembly"};
    auto write{ioj::FormatWriter{std::back_inserter(description)}};
    for (std::size_t part_index{}; part_index < parts.size(); ++part_index) {
        auto const& part{parts[part_index]};
        auto const& transform{part.transform};
        write(";part{}={{{};translation={};rotation={};scale={};visible={}}}",
              part_index,
              describe_request(part.mesh),
              format_vec(transform.translation),
              format_rotator(transform.rotation),
              format_vec(transform.scale),
              part.visible);
    }
    return description;
}

}
