#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

auto USbxMeshGenLabSettings::to_request() const -> FSbxMeshGenerationRequest {
    auto request{SandboxMesh::make_default_mesh_request(shape)};
    request.asset_name = asset_name;
    request.material_role = material_role;
    request.box.dimensions = FVector3f{static_cast<float>(box_dimensions.X),
                                       static_cast<float>(box_dimensions.Y),
                                       static_cast<float>(box_dimensions.Z)};
    request.beveled_box = {FVector3f{beveled_box_dimensions}, beveled_box_bevel_width};
    request.wedge = {FVector3f{wedge_dimensions}, wedge_top_length, wedge_top_offset};
    request.cylinder = {cylinder_radius, cylinder_height, cylinder_radial_segments};
    request.sphere = {sphere_radius, sphere_longitude_segments, sphere_latitude_segments};
    request.cone = {cone_radius, cone_height, cone_radial_segments};
    request.hex_tile = {
        hex_tile_outer_radius, hex_tile_depth, hex_tile_bevel_width, hex_tile_pointy_top};
    request.hex_frame = {
        hex_frame_outer_radius, hex_frame_wall_thickness, hex_frame_depth, hex_frame_pointy_top};
    request.honeycomb_panel = {honeycomb_rows,
                               honeycomb_columns,
                               honeycomb_cell_radius,
                               honeycomb_wall_thickness,
                               honeycomb_depth,
                               honeycomb_pointy_top};
    return request;
}

auto USbxMeshGenLabSettings::to_transform() const -> FSbxMeshTransform {
    return {FVector3f{part_translation}, FRotator3f{part_rotation}, FVector3f{part_scale}};
}

void USbxMeshGenLabSettings::load_request(FSbxMeshGenerationRequest const& request) {
    shape = request.shape;
    asset_name = request.asset_name;
    material_role = request.material_role;
    box_dimensions = FVector{request.box.dimensions};
    beveled_box_dimensions = FVector{request.beveled_box.dimensions};
    beveled_box_bevel_width = request.beveled_box.bevel_width;
    wedge_dimensions = FVector{request.wedge.dimensions};
    wedge_top_length = request.wedge.top_length;
    wedge_top_offset = request.wedge.top_offset;
    cylinder_radius = request.cylinder.radius;
    cylinder_height = request.cylinder.height;
    cylinder_radial_segments = request.cylinder.radial_segments;
    sphere_radius = request.sphere.radius;
    sphere_longitude_segments = request.sphere.longitude_segments;
    sphere_latitude_segments = request.sphere.latitude_segments;
    cone_radius = request.cone.radius;
    cone_height = request.cone.height;
    cone_radial_segments = request.cone.radial_segments;
    hex_tile_outer_radius = request.hex_tile.outer_radius;
    hex_tile_depth = request.hex_tile.depth;
    hex_tile_bevel_width = request.hex_tile.bevel_width;
    hex_tile_pointy_top = request.hex_tile.pointy_top;
    hex_frame_outer_radius = request.hex_frame.outer_radius;
    hex_frame_wall_thickness = request.hex_frame.wall_thickness;
    hex_frame_depth = request.hex_frame.depth;
    hex_frame_pointy_top = request.hex_frame.pointy_top;
    honeycomb_rows = request.honeycomb_panel.rows;
    honeycomb_columns = request.honeycomb_panel.columns;
    honeycomb_cell_radius = request.honeycomb_panel.cell_radius;
    honeycomb_wall_thickness = request.honeycomb_panel.wall_thickness;
    honeycomb_depth = request.honeycomb_panel.depth;
    honeycomb_pointy_top = request.honeycomb_panel.pointy_top;
}

void USbxMeshGenLabSettings::load_transform(FSbxMeshTransform const& transform) {
    part_translation = FVector{transform.translation};
    part_rotation = FRotator{transform.rotation};
    part_scale = FVector{transform.scale};
}
