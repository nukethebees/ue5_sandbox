#pragma once

#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include <expected>

namespace SandboxMesh {

template <typename Input>
[[nodiscard]] auto try_make_mesh_request(Input const& input, FName const asset_name)
    -> std::expected<FSbxMeshGenerationRequest, FString> {
    FSbxMeshGenerationRequest request{
        .shape = to_native(input.shape),
        .asset_name = asset_name.IsNone() ? std::string{} : to_native(asset_name),
        .material_role = to_native(input.material_role),
        .box = {to_native(input.box_dimensions)},
        .cylinder = {input.cylinder_radius, input.cylinder_height, input.cylinder_radial_segments},
        .sphere = {input.sphere_radius,
                   input.sphere_longitude_segments,
                   input.sphere_latitude_segments},
        .cone = {input.cone_radius, input.cone_height, input.cone_radial_segments},
        .hex_tile = {input.hex_tile_outer_radius,
                     input.hex_tile_depth,
                     input.hex_tile_bevel_width,
                     input.hex_tile_pointy_top},
        .hex_frame = {input.hex_frame_outer_radius,
                      input.hex_frame_wall_thickness,
                      input.hex_frame_depth,
                      input.hex_frame_pointy_top},
        .honeycomb_panel = {input.honeycomb_rows,
                            input.honeycomb_columns,
                            input.honeycomb_cell_radius,
                            input.honeycomb_wall_thickness,
                            input.honeycomb_depth,
                            input.honeycomb_pointy_top},
        .beveled_box = {to_native(input.beveled_box_dimensions), input.beveled_box_bevel_width},
        .wedge = {to_native(input.wedge_dimensions),
                  input.wedge_top_length,
                  input.wedge_top_offset},
    };
    if (auto const error{mesh_gen::validate_request(request)}; !error.empty()) {
        return std::unexpected{FString{UTF8_TO_TCHAR(error.data())}};
    }
    return request;
}

}
