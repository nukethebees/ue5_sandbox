#pragma once

#include <mesh_gen/beveled_box_parameters.h>
#include <mesh_gen/box_parameters.h>
#include <mesh_gen/cone_parameters.h>
#include <mesh_gen/cylinder_parameters.h>
#include <mesh_gen/hex_frame_parameters.h>
#include <mesh_gen/hex_tile_parameters.h>
#include <mesh_gen/honeycomb_panel_parameters.h>
#include <mesh_gen/material_role.h>
#include <mesh_gen/shape.h>
#include <mesh_gen/sphere_parameters.h>
#include <mesh_gen/wedge_parameters.h>

#include <string>
#include <string_view>

namespace mesh_gen {

struct GenerationRequest {
    Shape shape{Shape::box};
    std::string asset_name{"SM_GeneratedBox"};
    MaterialRole material_role{MaterialRole::structure};
    BoxParameters box{};
    CylinderParameters cylinder{};
    SphereParameters sphere{};
    ConeParameters cone{};
    HexTileParameters hex_tile{};
    HexFrameParameters hex_frame{};
    HoneycombPanelParameters honeycomb_panel{};
    BeveledBoxParameters beveled_box{};
    WedgeParameters wedge{};
};

[[nodiscard]] auto make_default_request(Shape shape) -> GenerationRequest;
[[nodiscard]] auto validate_request(GenerationRequest const& request) -> std::string_view;
// Validate authored requests before passing them to generation algorithms.
[[nodiscard]] auto generate_mesh(GenerationRequest const& request) -> MeshData;
[[nodiscard]] auto describe_request(GenerationRequest const& request) -> std::string;

}
