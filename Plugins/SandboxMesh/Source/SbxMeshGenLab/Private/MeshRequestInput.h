#pragma once

#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include <expected>

namespace SandboxMesh {

// Validate editable settings and recipe fields before publishing a generation request.
template <typename Input>
[[nodiscard]] auto try_make_mesh_request(Input const& input, FName const asset_name)
    -> std::expected<FSbxMeshGenerationRequest, FString> {
    using PositiveFloat = ioj::PositiveFinite<float>;

    auto request{make_default_mesh_request(input.shape)};
    request.asset_name = TCHAR_TO_UTF8(*asset_name.ToString());
    request.material_role = to_native(input.material_role);

    switch (input.shape) {
        case ESbxMeshShape::Box:
            request.box.dimensions = to_native(input.box_dimensions);
            break;
        case ESbxMeshShape::BeveledBox: {
            auto const bevel_width{PositiveFloat::try_make(input.beveled_box_bevel_width)};
            if (!bevel_width) {
                return std::unexpected{
                    FString{TEXT("Beveled-box bevel width must be finite and positive.")}};
            }
            request.beveled_box = {to_native(input.beveled_box_dimensions), *bevel_width};
            break;
        }
        case ESbxMeshShape::Wedge: {
            auto const top_length{PositiveFloat::try_make(input.wedge_top_length)};
            if (!top_length) {
                return std::unexpected{
                    FString{TEXT("Wedge top length must be finite and positive.")}};
            }
            request.wedge = {
                to_native(input.wedge_dimensions), *top_length, input.wedge_top_offset};
            break;
        }
        case ESbxMeshShape::Cylinder: {
            auto const radius{PositiveFloat::try_make(input.cylinder_radius)};
            auto const height{PositiveFloat::try_make(input.cylinder_height)};
            if (!radius || !height) {
                return std::unexpected{
                    FString{TEXT("Cylinder radius and height must be finite and positive.")}};
            }
            request.cylinder = {*radius, *height, input.cylinder_radial_segments};
            break;
        }
        case ESbxMeshShape::Sphere: {
            auto const radius{PositiveFloat::try_make(input.sphere_radius)};
            if (!radius) {
                return std::unexpected{FString{TEXT("Sphere radius must be finite and positive.")}};
            }
            request.sphere = {
                *radius, input.sphere_longitude_segments, input.sphere_latitude_segments};
            break;
        }
        case ESbxMeshShape::Cone: {
            auto const radius{PositiveFloat::try_make(input.cone_radius)};
            auto const height{PositiveFloat::try_make(input.cone_height)};
            if (!radius || !height) {
                return std::unexpected{
                    FString{TEXT("Cone radius and height must be finite and positive.")}};
            }
            request.cone = {*radius, *height, input.cone_radial_segments};
            break;
        }
        case ESbxMeshShape::HexTile: {
            auto const radius{PositiveFloat::try_make(input.hex_tile_outer_radius)};
            auto const depth{PositiveFloat::try_make(input.hex_tile_depth)};
            auto const bevel_width{PositiveFloat::try_make(input.hex_tile_bevel_width)};
            if (!radius || !depth || !bevel_width) {
                return std::unexpected{FString{
                    TEXT("Hex-tile radius, depth, and bevel width must be finite and positive.")}};
            }
            request.hex_tile = {*radius, *depth, *bevel_width, input.hex_tile_pointy_top};
            break;
        }
        case ESbxMeshShape::HexFrame: {
            auto const radius{PositiveFloat::try_make(input.hex_frame_outer_radius)};
            auto const wall_thickness{PositiveFloat::try_make(input.hex_frame_wall_thickness)};
            auto const depth{PositiveFloat::try_make(input.hex_frame_depth)};
            if (!radius || !wall_thickness || !depth) {
                return std::unexpected{FString{TEXT(
                    "Hex-frame radius, wall thickness, and depth must be finite and positive.")}};
            }
            request.hex_frame = {*radius, *wall_thickness, *depth, input.hex_frame_pointy_top};
            break;
        }
        case ESbxMeshShape::HoneycombPanel: {
            auto const rows{ioj::Positive<int32>::try_make(input.honeycomb_rows)};
            auto const columns{ioj::Positive<int32>::try_make(input.honeycomb_columns)};
            auto const cell_radius{PositiveFloat::try_make(input.honeycomb_cell_radius)};
            auto const wall_thickness{PositiveFloat::try_make(input.honeycomb_wall_thickness)};
            auto const depth{PositiveFloat::try_make(input.honeycomb_depth)};
            if (!rows || !columns || !cell_radius || !wall_thickness || !depth) {
                return std::unexpected{
                    FString{TEXT("Honeycomb rows and columns must be positive; radius, wall "
                                 "thickness, and depth must be finite and positive.")}};
            }
            request.honeycomb_panel = {
                *rows, *columns, *cell_radius, *wall_thickness, *depth, input.honeycomb_pointy_top};
            break;
        }
        default:
            return std::unexpected{FString{TEXT("Mesh shape is invalid.")}};
    }

    auto const error{mesh_gen::validate_request(request)};
    if (!error.empty()) {
        return std::unexpected{FString{UTF8_TO_TCHAR(error.c_str())}};
    }
    return request;
}

}
