#include <mesh_gen/MeshGeneration.h>

#include "MeshMath.h"
#include <ioj/format_writer.h>

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
            write("box;dimensions={}", format_vec(request.box.ref().dimensions));
            break;
        case Shape::cylinder:
            write("cylinder;radius={};height={};segments={}",
                  format_number(request.cylinder.ref().radius),
                  format_number(request.cylinder.ref().height),
                  request.cylinder.ref().radial_segments);
            break;
        case Shape::sphere:
            write("sphere;radius={};longitude_segments={};latitude_segments={}",
                  format_number(request.sphere.ref().radius),
                  request.sphere.ref().longitude_segments,
                  request.sphere.ref().latitude_segments);
            break;
        case Shape::cone:
            write("cone;radius={};height={};segments={}",
                  format_number(request.cone.ref().radius),
                  format_number(request.cone.ref().height),
                  request.cone.ref().radial_segments);
            break;
        case Shape::hex_tile:
            write("hex_tile;outer_radius={};depth={};bevel_width={};pointy_top={}",
                  format_number(request.hex_tile.ref().outer_radius),
                  format_number(request.hex_tile.ref().depth),
                  format_number(request.hex_tile.ref().bevel_width),
                  request.hex_tile.ref().pointy_top);
            break;
        case Shape::hex_frame:
            write("hex_frame;outer_radius={};wall_thickness={};depth={};pointy_top={};uv_mode={}",
                  format_number(request.hex_frame.ref().outer_radius),
                  format_number(request.hex_frame.ref().wall_thickness),
                  format_number(request.hex_frame.ref().depth),
                  request.hex_frame.ref().pointy_top,
                  request.hex_frame.ref().uv_mode == HexFrameUvMode::perimeter ? "perimeter"
                                                                               : "per_face");
            break;
        case Shape::honeycomb_panel:
            write("honeycomb_panel;rows={};columns={};cell_radius={};wall_thickness={};"
                  "depth={};pointy_top={}",
                  request.honeycomb_panel.ref().rows,
                  request.honeycomb_panel.ref().columns,
                  format_number(request.honeycomb_panel.ref().cell_radius),
                  format_number(request.honeycomb_panel.ref().wall_thickness),
                  format_number(request.honeycomb_panel.ref().depth),
                  request.honeycomb_panel.ref().pointy_top);
            break;
        case Shape::beveled_box:
            write("beveled_box;dimensions={};bevel_width={}",
                  format_vec(request.beveled_box.ref().dimensions),
                  format_number(request.beveled_box.ref().bevel_width));
            break;
        case Shape::wedge:
            write("wedge;dimensions={};top_length={};top_offset={}",
                  format_vec(request.wedge.ref().dimensions),
                  format_number(request.wedge.ref().top_length),
                  format_number(request.wedge.ref().top_offset));
            break;
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
                             Transform const& checked) {
    auto const& transform{checked.ref()};
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

auto generate_assembly(std::span<AssemblyPart const> const parts) -> MeshData {
    assert(!parts.empty());
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
        write(";part{}={{{};translation={};rotation={};scale={};visible={}}}",
              part_index,
              describe_request(part.mesh),
              format_vec(part.transform.ref().translation),
              format_rotator(part.transform.ref().rotation),
              format_vec(part.transform.ref().scale),
              part.visible);
    }
    return description;
}

}
