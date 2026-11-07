#pragma once

#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include <expected>

namespace SandboxMesh {

template <typename Input>
[[nodiscard]] auto try_make_mesh_request(Input const& input, FName const asset_name)
    -> std::expected<FSbxMeshGenerationRequest, FString> {
    auto request{make_default_mesh_request(input.shape)};
    request.asset_name = TCHAR_TO_UTF8(*asset_name.ToString());
    request.material_role = to_native(input.material_role);

    auto const assign{
        []<typename Parameters>(Parameters& target, typename Parameters::value_type const& fields) {
            if (auto const checked{Parameters::try_make(fields)}) {
                target = *checked;
                return FString{};
            }
            return FString{UTF8_TO_TCHAR(fields.error().data())};
        }};

    FString error;
    switch (input.shape) {
        case ESbxMeshShape::Box:
            error = assign(request.box, {to_native(input.box_dimensions)});
            break;
        case ESbxMeshShape::BeveledBox:
            error =
                assign(request.beveled_box,
                       {to_native(input.beveled_box_dimensions), input.beveled_box_bevel_width});
            break;
        case ESbxMeshShape::Wedge:
            error = assign(request.wedge,
                           {to_native(input.wedge_dimensions),
                            input.wedge_top_length,
                            input.wedge_top_offset});
            break;
        case ESbxMeshShape::Cylinder:
            error = assign(
                request.cylinder,
                {input.cylinder_radius, input.cylinder_height, input.cylinder_radial_segments});
            break;
        case ESbxMeshShape::Sphere:
            error = assign(request.sphere,
                           {input.sphere_radius,
                            input.sphere_longitude_segments,
                            input.sphere_latitude_segments});
            break;
        case ESbxMeshShape::Cone:
            error = assign(request.cone,
                           {input.cone_radius, input.cone_height, input.cone_radial_segments});
            break;
        case ESbxMeshShape::HexTile:
            error = assign(request.hex_tile,
                           {input.hex_tile_outer_radius,
                            input.hex_tile_depth,
                            input.hex_tile_bevel_width,
                            input.hex_tile_pointy_top});
            break;
        case ESbxMeshShape::HexFrame:
            error = assign(request.hex_frame,
                           {input.hex_frame_outer_radius,
                            input.hex_frame_wall_thickness,
                            input.hex_frame_depth,
                            input.hex_frame_pointy_top});
            break;
        case ESbxMeshShape::HoneycombPanel:
            error = assign(request.honeycomb_panel,
                           {input.honeycomb_rows,
                            input.honeycomb_columns,
                            input.honeycomb_cell_radius,
                            input.honeycomb_wall_thickness,
                            input.honeycomb_depth,
                            input.honeycomb_pointy_top});
            break;
        default:
            return std::unexpected{FString{TEXT("Mesh shape is invalid.")}};
    }
    if (!error.IsEmpty()) {
        return std::unexpected{std::move(error)};
    }
    return request;
}

}
