#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

#include "MeshRequestInput.h"

auto FSbxMeshAssemblyRecipePart::from_part(FSbxMeshAssemblyPart const& part,
                                           FGuid const id,
                                           FGuid const parent_id) -> FSbxMeshAssemblyRecipePart {
    FSbxMeshAssemblyRecipePart recipe_part;
    recipe_part.id = id;
    recipe_part.parent_id = parent_id;
    recipe_part.visible = part.visible;
    recipe_part.locked = part.locked;
    recipe_part.shape = SandboxMesh::to_unreal(part.mesh.shape);
    recipe_part.material_role = SandboxMesh::to_unreal(part.mesh.material_role);
    auto const& transform{part.transform};
    recipe_part.translation = SandboxMesh::to_unreal(transform.translation);
    recipe_part.rotation = SandboxMesh::to_unreal(transform.rotation);
    recipe_part.scale = SandboxMesh::to_unreal(transform.scale);
    recipe_part.box_dimensions = SandboxMesh::to_unreal(part.mesh.box.dimensions);

    auto const& beveled_box{part.mesh.beveled_box};
    recipe_part.beveled_box_dimensions = SandboxMesh::to_unreal(beveled_box.dimensions);
    recipe_part.beveled_box_bevel_width = beveled_box.bevel_width;

    auto const& wedge{part.mesh.wedge};
    recipe_part.wedge_dimensions = SandboxMesh::to_unreal(wedge.dimensions);
    recipe_part.wedge_top_length = wedge.top_length;
    recipe_part.wedge_top_offset = wedge.top_offset;

    auto const& cylinder{part.mesh.cylinder};
    recipe_part.cylinder_radius = cylinder.radius;
    recipe_part.cylinder_height = cylinder.height;
    recipe_part.cylinder_radial_segments = cylinder.radial_segments;

    auto const& sphere{part.mesh.sphere};
    recipe_part.sphere_radius = sphere.radius;
    recipe_part.sphere_longitude_segments = sphere.longitude_segments;
    recipe_part.sphere_latitude_segments = sphere.latitude_segments;

    auto const& cone{part.mesh.cone};
    recipe_part.cone_radius = cone.radius;
    recipe_part.cone_height = cone.height;
    recipe_part.cone_radial_segments = cone.radial_segments;

    auto const& hex_tile{part.mesh.hex_tile};
    recipe_part.hex_tile_outer_radius = hex_tile.outer_radius;
    recipe_part.hex_tile_depth = hex_tile.depth;
    recipe_part.hex_tile_bevel_width = hex_tile.bevel_width;
    recipe_part.hex_tile_pointy_top = hex_tile.pointy_top;

    auto const& hex_frame{part.mesh.hex_frame};
    recipe_part.hex_frame_outer_radius = hex_frame.outer_radius;
    recipe_part.hex_frame_wall_thickness = hex_frame.wall_thickness;
    recipe_part.hex_frame_depth = hex_frame.depth;
    recipe_part.hex_frame_pointy_top = hex_frame.pointy_top;

    auto const& honeycomb_panel{part.mesh.honeycomb_panel};
    recipe_part.honeycomb_rows = honeycomb_panel.rows;
    recipe_part.honeycomb_columns = honeycomb_panel.columns;
    recipe_part.honeycomb_cell_radius = honeycomb_panel.cell_radius;
    recipe_part.honeycomb_wall_thickness = honeycomb_panel.wall_thickness;
    recipe_part.honeycomb_depth = honeycomb_panel.depth;
    recipe_part.honeycomb_pointy_top = honeycomb_panel.pointy_top;
    return recipe_part;
}

auto FSbxMeshAssemblyRecipePart::to_part(FName const output_asset_name) const
    -> std::expected<FSbxMeshAssemblyPart, FString> {
    auto request{SandboxMesh::try_make_mesh_request(*this, output_asset_name)};
    if (!request) {
        return std::unexpected{request.error()};
    }
    auto const transform{to_transform()};
    if (auto const error{mesh_gen::validate_transform(transform)}; !error.empty()) {
        return std::unexpected{FString{UTF8_TO_TCHAR(error.data())}};
    }
    return FSbxMeshAssemblyPart{std::move(*request), transform, visible, locked};
}

auto FSbxMeshAssemblyRecipePart::to_transform() const -> FSbxMeshTransform {
    return {SandboxMesh::to_native(translation),
            SandboxMesh::to_native(rotation),
            SandboxMesh::to_native(scale)};
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
                              FGuid& parent_id,
                              bool& visible,
                              bool& locked) -> bool {
    if (auto const* const group_index{group_indices.Find(id)}; group_index != nullptr) {
        auto const& group{groups[*group_index]};
        transform = group.to_transform();
        parent_id = group.parent_id;
        visible = group.visible;
        locked = group.locked;
        return true;
    }

    auto const* const part_index{part_indices.Find(id)};
    if (part_index == nullptr) {
        return false;
    }

    auto const part_transform{parts[*part_index].to_transform()};
    transform = FTransform{SandboxMesh::to_unreal(part_transform.rotation),
                           SandboxMesh::to_unreal(part_transform.translation),
                           SandboxMesh::to_unreal(part_transform.scale)};
    parent_id = parts[*part_index].parent_id;
    visible = parts[*part_index].visible;
    locked = parts[*part_index].locked;
    return true;
}

auto resolve_node_transform(FGuid const id,
                            TArray<FSbxMeshAssemblyRecipePart> const& parts,
                            TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                            TMap<FGuid, int32> const& part_indices,
                            TMap<FGuid, int32> const& group_indices,
                            TMap<FGuid, uint8>& states,
                            TMap<FGuid, FTransform>& transforms,
                            TMap<FGuid, bool>& visibilities,
                            TMap<FGuid, bool>& locks) -> bool {
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
    bool visible{};
    bool locked{};
    if (!get_node_local_transform(id,
                                  parts,
                                  groups,
                                  part_indices,
                                  group_indices,
                                  transform,
                                  parent_id,
                                  visible,
                                  locked)) {
        return false;
    }

    if (parent_id.IsValid()) {
        if (!resolve_node_transform(parent_id,
                                    parts,
                                    groups,
                                    part_indices,
                                    group_indices,
                                    states,
                                    transforms,
                                    visibilities,
                                    locks)) {
            return false;
        }
        transform *= transforms.FindChecked(parent_id);
        visible = visible && visibilities.FindChecked(parent_id);
        locked = locked || locks.FindChecked(parent_id);
    }
    transforms.Add(id, transform);
    visibilities.Add(id, visible);
    locks.Add(id, locked);
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

auto resolve_mesh_assembly_hierarchy(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                                     FName const output_asset_name)
    -> std::expected<TArray<FSbxMeshAssemblyPart>, FString> {
    TSet<FGuid> node_ids;
    TMap<FGuid, int32> group_indices;
    TMap<FGuid, int32> part_indices;
    TArray<FSbxMeshAssemblyPart> resolved_parts;
    resolved_parts.Reserve(parts.Num());
    auto const group_count{groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        auto const& group{groups[group_index]};
        if (!group.id.IsValid()) {
            return std::unexpected{
                FString::Printf(TEXT("Group %d has no stable ID."), group_index + 1)};
        }
        if (node_ids.Contains(group.id)) {
            return std::unexpected{FString{TEXT("Assembly node IDs must be unique.")}};
        }
        auto const transform_error{
            mesh_gen::validate_transform({SandboxMesh::to_native(group.translation),
                                          SandboxMesh::to_native(group.rotation),
                                          SandboxMesh::to_native(group.scale)})};
        if (!transform_error.empty()) {
            return std::unexpected{FString::Printf(
                TEXT("Group %d: %s"), group_index + 1, UTF8_TO_TCHAR(transform_error.data()))};
        }
        TSet<FName> connector_names;
        for (auto const& connector : group.connectors) {
            if (connector.name.IsNone()) {
                return std::unexpected{FString::Printf(TEXT("Group '%s' has an unnamed connector."),
                                                       *group.name.ToString())};
            }
            if (connector_names.Contains(connector.name)) {
                return std::unexpected{FString::Printf(
                    TEXT("Group '%s' has duplicate connector names."), *group.name.ToString())};
            }
            if (connector.translation.ContainsNaN() || connector.rotation.ContainsNaN()) {
                return std::unexpected{
                    FString::Printf(TEXT("Group '%s' has an invalid connector transform."),
                                    *group.name.ToString())};
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
            return std::unexpected{
                FString::Printf(TEXT("Part %d has no stable ID."), part_index + 1)};
        }
        if (node_ids.Contains(part.id)) {
            return std::unexpected{FString{TEXT("Assembly node IDs must be unique.")}};
        }
        auto validated_part{part.to_part(output_asset_name)};
        if (!validated_part) {
            return std::unexpected{
                FString::Printf(TEXT("Part %d: %s"), part_index + 1, *validated_part.error())};
        }
        resolved_parts.Add(MoveTemp(*validated_part));
        node_ids.Add(part.id);
        part_indices.Add(part.id, part_index);
    }

    for (auto const& group : groups) {
        if (group.parent_id.IsValid() && !node_ids.Contains(group.parent_id)) {
            return std::unexpected{
                FString::Printf(TEXT("Group '%s' has a missing parent."), *group.name.ToString())};
        }
    }
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        if (part.parent_id.IsValid() && !node_ids.Contains(part.parent_id)) {
            return std::unexpected{
                FString::Printf(TEXT("Part %d has a missing parent."), part_index + 1)};
        }
    }

    TMap<FGuid, uint8> states;
    TMap<FGuid, FTransform> transforms;
    TMap<FGuid, bool> visibilities;
    TMap<FGuid, bool> locks;
    for (FGuid const id : node_ids) {
        if (!resolve_node_transform(id,
                                    parts,
                                    groups,
                                    part_indices,
                                    group_indices,
                                    states,
                                    transforms,
                                    visibilities,
                                    locks)) {
            return std::unexpected{FString{TEXT("Assembly hierarchy contains a parenting cycle.")}};
        }
    }

    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& recipe_part{parts[part_index]};
        auto& part{resolved_parts[part_index]};
        auto const& transform{transforms.FindChecked(recipe_part.id)};
        auto const input{mesh_gen::Transform{SandboxMesh::to_native(transform.GetLocation()),
                                             SandboxMesh::to_native(transform.Rotator()),
                                             SandboxMesh::to_native(transform.GetScale3D())}};
        if (auto const error{mesh_gen::validate_transform(input)}; !error.empty()) {
            return std::unexpected{
                FString::Printf(TEXT("Part %d: %s"), part_index + 1, UTF8_TO_TCHAR(error.data()))};
        }
        part.transform = input;
        part.visible = visibilities.FindChecked(recipe_part.id);
        part.locked = locks.FindChecked(recipe_part.id);
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
