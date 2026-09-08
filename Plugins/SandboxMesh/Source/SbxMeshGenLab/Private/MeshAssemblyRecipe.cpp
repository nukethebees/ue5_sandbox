#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

auto FSbxMeshAssemblyRecipePart::from_part(FSbxMeshAssemblyPart const& part,
                                           FGuid const id,
                                           FGuid const parent_id) -> FSbxMeshAssemblyRecipePart {
    FSbxMeshAssemblyRecipePart recipe_part;
    recipe_part.id = id;
    recipe_part.parent_id = parent_id;
    recipe_part.shape = part.mesh.shape;
    recipe_part.material_role = part.mesh.material_role;
    recipe_part.translation = FVector{part.transform.translation};
    recipe_part.rotation = FRotator{part.transform.rotation};
    recipe_part.scale = FVector{part.transform.scale};
    recipe_part.box_dimensions = FVector{part.mesh.box.dimensions};
    recipe_part.beveled_box_dimensions = FVector{part.mesh.beveled_box.dimensions};
    recipe_part.beveled_box_bevel_width = part.mesh.beveled_box.bevel_width;
    recipe_part.wedge_dimensions = FVector{part.mesh.wedge.dimensions};
    recipe_part.wedge_top_length = part.mesh.wedge.top_length;
    recipe_part.wedge_top_offset = part.mesh.wedge.top_offset;
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
    request.material_role = material_role;
    request.box.dimensions = FVector3f{box_dimensions};
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

    return {request, {FVector3f{translation}, FRotator3f{rotation}, FVector3f{scale}}};
}

auto FSbxMeshAssemblyRecipeGroup::to_transform() const -> FTransform {
    return FTransform{rotation, translation, scale};
}

auto FSbxMeshAssemblyConnector::to_transform() const -> FTransform {
    return FTransform{rotation, translation, FVector::OneVector};
}

void FSbxMeshAssemblyRecipeGroup::set_transform(FTransform const& transform) {
    translation = transform.GetLocation();
    rotation = transform.Rotator();
    scale = transform.GetScale3D();
}

namespace SandboxMesh {
namespace {
auto get_node_local_transform(FGuid const id,
                              TArray<FSbxMeshAssemblyRecipePart> const& parts,
                              TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                              TMap<FGuid, int32> const& part_indices,
                              TMap<FGuid, int32> const& group_indices,
                              FTransform& transform,
                              FGuid& parent_id) -> bool {
    if (auto const* const group_index{group_indices.Find(id)}; group_index != nullptr) {
        auto const& group{groups[*group_index]};
        transform = group.to_transform();
        parent_id = group.parent_id;
        return true;
    }

    auto const* const part_index{part_indices.Find(id)};
    if (part_index == nullptr) {
        return false;
    }

    auto const part{parts[*part_index].to_part(NAME_None)};
    transform = FTransform{FRotator{part.transform.rotation},
                           FVector{part.transform.translation},
                           FVector{part.transform.scale}};
    parent_id = parts[*part_index].parent_id;
    return true;
}

auto resolve_node_transform(FGuid const id,
                            TArray<FSbxMeshAssemblyRecipePart> const& parts,
                            TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                            TMap<FGuid, int32> const& part_indices,
                            TMap<FGuid, int32> const& group_indices,
                            TMap<FGuid, uint8>& states,
                            TMap<FGuid, FTransform>& transforms) -> bool {
    auto& state{states.FindOrAdd(id)};
    if (state == 2) {
        return true;
    }
    if (state == 1) {
        return false;
    }

    state = 1;
    FTransform transform;
    FGuid parent_id;
    if (!get_node_local_transform(
            id, parts, groups, part_indices, group_indices, transform, parent_id)) {
        return false;
    }

    if (parent_id.IsValid()) {
        if (!resolve_node_transform(
                parent_id, parts, groups, part_indices, group_indices, states, transforms)) {
            return false;
        }
        transform *= transforms.FindChecked(parent_id);
    }
    transforms.Add(id, transform);
    state = 2;
    return true;
}
}

auto is_legacy_mesh_assembly_recipe(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                    TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                                    int32 const format_version) -> bool {
    if (format_version == 1) {
        return true;
    }
    if (!groups.IsEmpty() || parts.IsEmpty()) {
        return false;
    }
    return parts.ContainsByPredicate(
               [](FSbxMeshAssemblyRecipePart const& part) { return part.id.IsValid(); }) == false;
}

auto validate_mesh_assembly_hierarchy(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                      TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
    -> FString {
    TSet<FGuid> node_ids;
    TMap<FGuid, int32> group_indices;
    TMap<FGuid, int32> part_indices;
    auto const group_count{groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        auto const& group{groups[group_index]};
        if (!group.id.IsValid()) {
            return FString::Printf(TEXT("Group %d has no stable ID."), group_index + 1);
        }
        if (node_ids.Contains(group.id)) {
            return TEXT("Assembly node IDs must be unique.");
        }
        if (group.scale.GetMin() <= 0.0) {
            return FString::Printf(TEXT("Group %d scale values must be greater than zero."),
                                   group_index + 1);
        }
        TSet<FName> connector_names;
        for (auto const& connector : group.connectors) {
            if (connector.name.IsNone()) {
                return FString::Printf(TEXT("Group '%s' has an unnamed connector."),
                                       *group.name.ToString());
            }
            if (connector_names.Contains(connector.name)) {
                return FString::Printf(TEXT("Group '%s' has duplicate connector names."),
                                       *group.name.ToString());
            }
            if (connector.translation.ContainsNaN() || connector.rotation.ContainsNaN()) {
                return FString::Printf(TEXT("Group '%s' has an invalid connector transform."),
                                       *group.name.ToString());
            }
            connector_names.Add(connector.name);
        }
        node_ids.Add(group.id);
        group_indices.Add(group.id, group_index);
    }

    auto const part_count{parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        if (!part.id.IsValid()) {
            return FString::Printf(TEXT("Part %d has no stable ID."), part_index + 1);
        }
        if (node_ids.Contains(part.id)) {
            return TEXT("Assembly node IDs must be unique.");
        }
        node_ids.Add(part.id);
        part_indices.Add(part.id, part_index);
    }

    for (auto const& group : groups) {
        if (group.parent_id.IsValid() && !node_ids.Contains(group.parent_id)) {
            return FString::Printf(TEXT("Group '%s' has a missing parent."),
                                   *group.name.ToString());
        }
    }
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        if (part.parent_id.IsValid() && !node_ids.Contains(part.parent_id)) {
            return FString::Printf(TEXT("Part %d has a missing parent."), part_index + 1);
        }
    }

    TMap<FGuid, uint8> states;
    TMap<FGuid, FTransform> transforms;
    for (FGuid const id : node_ids) {
        if (!resolve_node_transform(
                id, parts, groups, part_indices, group_indices, states, transforms)) {
            return TEXT("Assembly hierarchy contains a parenting cycle.");
        }
    }
    return {};
}

auto resolve_mesh_assembly_hierarchy(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                                     FName const output_asset_name)
    -> TArray<FSbxMeshAssemblyPart> {
    check(validate_mesh_assembly_hierarchy(parts, groups).IsEmpty());

    TMap<FGuid, int32> group_indices;
    TMap<FGuid, int32> part_indices;
    auto const group_count{groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        group_indices.Add(groups[group_index].id, group_index);
    }
    auto const part_count{parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        part_indices.Add(parts[part_index].id, part_index);
    }
    TMap<FGuid, uint8> states;
    TMap<FGuid, FTransform> transforms;
    for (auto const& group : groups) {
        check(resolve_node_transform(
            group.id, parts, groups, part_indices, group_indices, states, transforms));
    }
    for (auto const& part : parts) {
        check(resolve_node_transform(
            part.id, parts, groups, part_indices, group_indices, states, transforms));
    }

    TArray<FSbxMeshAssemblyPart> resolved_parts;
    resolved_parts.Reserve(parts.Num());
    for (auto const& recipe_part : parts) {
        auto part{recipe_part.to_part(output_asset_name)};
        auto const& transform{transforms.FindChecked(recipe_part.id)};
        part.transform = {FVector3f{transform.GetLocation()},
                          FRotator3f{transform.Rotator()},
                          FVector3f{transform.GetScale3D()}};
        resolved_parts.Add(MoveTemp(part));
    }
    return resolved_parts;
}

}

void USbxMeshAssemblyRecipe::set_hierarchy(
    FName const asset_name,
    TArray<FSbxMeshAssemblyRecipePart> const& assembly_parts,
    TArray<FSbxMeshAssemblyRecipeGroup> const& assembly_groups) {
    format_version = 3;
    output_asset_name = asset_name;
    parts = assembly_parts;
    groups = assembly_groups;
}

auto USbxMeshAssemblyRecipe::to_assembly() const -> TArray<FSbxMeshAssemblyPart> {
    auto hierarchy_parts{parts};
    auto const is_legacy{
        SandboxMesh::is_legacy_mesh_assembly_recipe(hierarchy_parts, groups, format_version)};
    if (is_legacy) {
        for (auto& part : hierarchy_parts) {
            part.id = FGuid::NewGuid();
            part.parent_id.Invalidate();
        }
    }
    return SandboxMesh::resolve_mesh_assembly_hierarchy(
        hierarchy_parts,
        is_legacy ? TArray<FSbxMeshAssemblyRecipeGroup>{} : groups,
        output_asset_name);
}
