#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "MeshRequestInput.h"

auto USbxMeshGenLabSettings::to_request() const
    -> std::expected<FSbxMeshGenerationRequest, FString> {
    return SandboxMesh::try_make_mesh_request(*this, asset_name);
}

auto USbxMeshGenLabSettings::to_transform() const -> FSbxMeshTransformInput {
    return {SandboxMesh::to_native(part_translation),
            SandboxMesh::to_native(part_rotation),
            SandboxMesh::to_native(part_scale)};
}

auto USbxMeshGenLabSettings::to_part() const -> std::expected<FSbxMeshAssemblyPart, FString> {
    auto request{to_request()};
    if (!request) {
        return std::unexpected{request.error()};
    }
    auto const input{to_transform()};
    auto const transform{mesh_gen::Transform::try_make(input)};
    if (!transform) {
        return std::unexpected{FString{UTF8_TO_TCHAR(input.error().data())}};
    }
    return FSbxMeshAssemblyPart{std::move(*request), *transform};
}

void USbxMeshGenLabSettings::load_request(FSbxMeshGenerationRequest const& request) {
    shape = SandboxMesh::to_unreal(request.shape);
    asset_name = FName{UTF8_TO_TCHAR(request.asset_name.c_str())};
    material_role = SandboxMesh::to_unreal(request.material_role);
    box_dimensions = SandboxMesh::to_unreal(request.box.ref().dimensions);
    beveled_box_dimensions = SandboxMesh::to_unreal(request.beveled_box.ref().dimensions);
    beveled_box_bevel_width = request.beveled_box.ref().bevel_width;
    wedge_dimensions = SandboxMesh::to_unreal(request.wedge.ref().dimensions);
    wedge_top_length = request.wedge.ref().top_length;
    wedge_top_offset = request.wedge.ref().top_offset;
    cylinder_radius = request.cylinder.ref().radius;
    cylinder_height = request.cylinder.ref().height;
    cylinder_radial_segments = request.cylinder.ref().radial_segments;
    sphere_radius = request.sphere.ref().radius;
    sphere_longitude_segments = request.sphere.ref().longitude_segments;
    sphere_latitude_segments = request.sphere.ref().latitude_segments;
    cone_radius = request.cone.ref().radius;
    cone_height = request.cone.ref().height;
    cone_radial_segments = request.cone.ref().radial_segments;
    hex_tile_outer_radius = request.hex_tile.ref().outer_radius;
    hex_tile_depth = request.hex_tile.ref().depth;
    hex_tile_bevel_width = request.hex_tile.ref().bevel_width;
    hex_tile_pointy_top = request.hex_tile.ref().pointy_top;
    hex_frame_outer_radius = request.hex_frame.ref().outer_radius;
    hex_frame_wall_thickness = request.hex_frame.ref().wall_thickness;
    hex_frame_depth = request.hex_frame.ref().depth;
    hex_frame_pointy_top = request.hex_frame.ref().pointy_top;
    honeycomb_rows = request.honeycomb_panel.ref().rows;
    honeycomb_columns = request.honeycomb_panel.ref().columns;
    honeycomb_cell_radius = request.honeycomb_panel.ref().cell_radius;
    honeycomb_wall_thickness = request.honeycomb_panel.ref().wall_thickness;
    honeycomb_depth = request.honeycomb_panel.ref().depth;
    honeycomb_pointy_top = request.honeycomb_panel.ref().pointy_top;
}

void USbxMeshGenLabSettings::load_transform(FSbxMeshTransformInput const& transform) {
    part_translation = SandboxMesh::to_unreal(transform.translation);
    part_rotation = SandboxMesh::to_unreal(transform.rotation);
    part_scale = SandboxMesh::to_unreal(transform.scale);
}
