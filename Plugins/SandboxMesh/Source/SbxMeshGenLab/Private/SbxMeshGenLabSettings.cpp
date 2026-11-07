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

    auto const& beveled_box{request.beveled_box.ref()};
    beveled_box_dimensions = SandboxMesh::to_unreal(beveled_box.dimensions);
    beveled_box_bevel_width = beveled_box.bevel_width;

    auto const& wedge{request.wedge.ref()};
    wedge_dimensions = SandboxMesh::to_unreal(wedge.dimensions);
    wedge_top_length = wedge.top_length;
    wedge_top_offset = wedge.top_offset;

    auto const& cylinder{request.cylinder.ref()};
    cylinder_radius = cylinder.radius;
    cylinder_height = cylinder.height;
    cylinder_radial_segments = cylinder.radial_segments;

    auto const& sphere{request.sphere.ref()};
    sphere_radius = sphere.radius;
    sphere_longitude_segments = sphere.longitude_segments;
    sphere_latitude_segments = sphere.latitude_segments;

    auto const& cone{request.cone.ref()};
    cone_radius = cone.radius;
    cone_height = cone.height;
    cone_radial_segments = cone.radial_segments;

    auto const& hex_tile{request.hex_tile.ref()};
    hex_tile_outer_radius = hex_tile.outer_radius;
    hex_tile_depth = hex_tile.depth;
    hex_tile_bevel_width = hex_tile.bevel_width;
    hex_tile_pointy_top = hex_tile.pointy_top;

    auto const& hex_frame{request.hex_frame.ref()};
    hex_frame_outer_radius = hex_frame.outer_radius;
    hex_frame_wall_thickness = hex_frame.wall_thickness;
    hex_frame_depth = hex_frame.depth;
    hex_frame_pointy_top = hex_frame.pointy_top;

    auto const& honeycomb_panel{request.honeycomb_panel.ref()};
    honeycomb_rows = honeycomb_panel.rows;
    honeycomb_columns = honeycomb_panel.columns;
    honeycomb_cell_radius = honeycomb_panel.cell_radius;
    honeycomb_wall_thickness = honeycomb_panel.wall_thickness;
    honeycomb_depth = honeycomb_panel.depth;
    honeycomb_pointy_top = honeycomb_panel.pointy_top;
}

void USbxMeshGenLabSettings::load_transform(FSbxMeshTransformInput const& transform) {
    part_translation = SandboxMesh::to_unreal(transform.translation);
    part_rotation = SandboxMesh::to_unreal(transform.rotation);
    part_scale = SandboxMesh::to_unreal(transform.scale);
}
