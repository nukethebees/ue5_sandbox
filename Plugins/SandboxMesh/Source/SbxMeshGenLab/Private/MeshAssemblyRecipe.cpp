#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

auto FSbxMeshAssemblyRecipePart::from_part(FSbxMeshAssemblyPart const& part)
    -> FSbxMeshAssemblyRecipePart {
    FSbxMeshAssemblyRecipePart recipe_part;
    recipe_part.shape = part.mesh.shape;
    recipe_part.translation = FVector{part.transform.translation};
    recipe_part.rotation = FRotator{part.transform.rotation};
    recipe_part.scale = FVector{part.transform.scale};
    recipe_part.box_dimensions = FVector{part.mesh.box.dimensions};
    recipe_part.cylinder_radius = part.mesh.cylinder.radius;
    recipe_part.cylinder_height = part.mesh.cylinder.height;
    recipe_part.cylinder_radial_segments = part.mesh.cylinder.radial_segments;
    recipe_part.sphere_radius = part.mesh.sphere.radius;
    recipe_part.sphere_longitude_segments = part.mesh.sphere.longitude_segments;
    recipe_part.sphere_latitude_segments = part.mesh.sphere.latitude_segments;
    recipe_part.cone_radius = part.mesh.cone.radius;
    recipe_part.cone_height = part.mesh.cone.height;
    recipe_part.cone_radial_segments = part.mesh.cone.radial_segments;
    recipe_part.hex_tile_outer_radius = part.mesh.hex_tile.outer_radius;
    recipe_part.hex_tile_depth = part.mesh.hex_tile.depth;
    recipe_part.hex_tile_bevel_width = part.mesh.hex_tile.bevel_width;
    recipe_part.hex_tile_pointy_top = part.mesh.hex_tile.pointy_top;
    recipe_part.hex_frame_outer_radius = part.mesh.hex_frame.outer_radius;
    recipe_part.hex_frame_wall_thickness = part.mesh.hex_frame.wall_thickness;
    recipe_part.hex_frame_depth = part.mesh.hex_frame.depth;
    recipe_part.hex_frame_pointy_top = part.mesh.hex_frame.pointy_top;
    recipe_part.honeycomb_rows = part.mesh.honeycomb_panel.rows;
    recipe_part.honeycomb_columns = part.mesh.honeycomb_panel.columns;
    recipe_part.honeycomb_cell_radius = part.mesh.honeycomb_panel.cell_radius;
    recipe_part.honeycomb_wall_thickness = part.mesh.honeycomb_panel.wall_thickness;
    recipe_part.honeycomb_depth = part.mesh.honeycomb_panel.depth;
    recipe_part.honeycomb_pointy_top = part.mesh.honeycomb_panel.pointy_top;
    return recipe_part;
}

auto FSbxMeshAssemblyRecipePart::to_part(FName const output_asset_name) const
    -> FSbxMeshAssemblyPart {
    auto request{SandboxMesh::make_default_mesh_request(shape)};
    request.asset_name = output_asset_name;
    request.box.dimensions = FVector3f{box_dimensions};
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

    return {request, {FVector3f{translation}, FRotator3f{rotation}, FVector3f{scale}}};
}

void USbxMeshAssemblyRecipe::set_assembly(FName const asset_name,
                                          TArray<FSbxMeshAssemblyPart> const& assembly_parts) {
    output_asset_name = asset_name;
    parts.Reset();
    parts.Reserve(assembly_parts.Num());
    for (auto const& part : assembly_parts) {
        parts.Add(FSbxMeshAssemblyRecipePart::from_part(part));
    }
}

auto USbxMeshAssemblyRecipe::to_assembly() const -> TArray<FSbxMeshAssemblyPart> {
    TArray<FSbxMeshAssemblyPart> assembly_parts;
    assembly_parts.Reserve(parts.Num());
    for (auto const& part : parts) {
        assembly_parts.Add(part.to_part(output_asset_name));
    }
    return assembly_parts;
}
