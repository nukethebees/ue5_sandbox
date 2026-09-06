#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "Misc/PackageName.h"

namespace SandboxMesh {

auto make_default_mesh_request(ESbxMeshShape const shape) -> FSbxMeshGenerationRequest {
    FSbxMeshGenerationRequest request{};
    request.shape = shape;
    switch (shape) {
        case ESbxMeshShape::Box:
            request.asset_name = TEXT("SM_GeneratedBox");
            break;
        case ESbxMeshShape::Cylinder:
            request.asset_name = TEXT("SM_GeneratedCylinder");
            break;
        case ESbxMeshShape::Sphere:
            request.asset_name = TEXT("SM_GeneratedSphere");
            break;
        case ESbxMeshShape::Cone:
            request.asset_name = TEXT("SM_GeneratedCone");
            break;
        case ESbxMeshShape::HexTile:
            request.asset_name = TEXT("SM_GeneratedHexTile");
            break;
        case ESbxMeshShape::HexFrame:
            request.asset_name = TEXT("SM_GeneratedHexFrame");
            break;
        case ESbxMeshShape::HoneycombPanel:
            request.asset_name = TEXT("SM_GeneratedHoneycombPanel");
            break;
        case ESbxMeshShape::BeveledBox:
            request.asset_name = TEXT("SM_GeneratedBeveledBox");
            break;
        case ESbxMeshShape::Wedge:
            request.asset_name = TEXT("SM_GeneratedWedge");
            break;
    }
    return request;
}

auto validate_mesh_request(FSbxMeshGenerationRequest const& request) -> FString {
    FText invalid_name_reason;
    auto const asset_name{request.asset_name.ToString()};
    auto const object_path{
        FString::Printf(TEXT("/SandboxMesh/MeshGenLab/Generated/%s.%s"), *asset_name, *asset_name)};
    if (request.asset_name.IsNone() ||
        !FPackageName::IsValidObjectPath(object_path, &invalid_name_reason)) {
        return FString::Printf(
            TEXT("Invalid asset name '%s': %s"), *asset_name, *invalid_name_reason.ToString());
    }

    switch (request.shape) {
        case ESbxMeshShape::Box:
            if (request.box.dimensions.GetMin() <= 0.0f) {
                return TEXT("All box dimensions must be greater than zero.");
            }
            break;
        case ESbxMeshShape::Cylinder:
            if (request.cylinder.radius <= 0.0f || request.cylinder.height <= 0.0f ||
                request.cylinder.radial_segments < 3) {
                return TEXT(
                    "Cylinder radius and height must be positive, with at least 3 segments.");
            }
            break;
        case ESbxMeshShape::Sphere:
            if (request.sphere.radius <= 0.0f || request.sphere.longitude_segments < 3 ||
                request.sphere.latitude_segments < 2) {
                return TEXT("Sphere radius must be positive, with at least 3 longitude and 2 "
                            "latitude segments.");
            }
            break;
        case ESbxMeshShape::Cone:
            if (request.cone.radius <= 0.0f || request.cone.height <= 0.0f ||
                request.cone.radial_segments < 3) {
                return TEXT("Cone radius and height must be positive, with at least 3 segments.");
            }
            break;
        case ESbxMeshShape::HexTile:
            if (request.hex_tile.outer_radius <= 0.0f || request.hex_tile.depth <= 0.0f ||
                request.hex_tile.bevel_width <= 0.0f) {
                return TEXT("Hex-tile radius, depth, and bevel width must be positive.");
            }
            if (request.hex_tile.bevel_width >= request.hex_tile.outer_radius ||
                request.hex_tile.bevel_width >= request.hex_tile.depth * 0.5f) {
                return TEXT("Hex-tile bevel width must be less than its radius and half-depth.");
            }
            break;
        case ESbxMeshShape::HexFrame:
            if (request.hex_frame.outer_radius <= 0.0f ||
                request.hex_frame.wall_thickness <= 0.0f || request.hex_frame.depth <= 0.0f) {
                return TEXT("Hex-frame radius, wall thickness, and depth must be positive.");
            }
            if (request.hex_frame.wall_thickness >= request.hex_frame.outer_radius) {
                return TEXT("Hex-frame wall thickness must be less than its outer radius.");
            }
            break;
        case ESbxMeshShape::HoneycombPanel:
            if (request.honeycomb_panel.rows < 1 || request.honeycomb_panel.columns < 1 ||
                request.honeycomb_panel.cell_radius <= 0.0f ||
                request.honeycomb_panel.wall_thickness <= 0.0f ||
                request.honeycomb_panel.depth <= 0.0f) {
                return TEXT("Honeycomb rows and columns must be at least 1; radius, wall "
                            "thickness, and depth must be positive.");
            }
            if (request.honeycomb_panel.wall_thickness >= request.honeycomb_panel.cell_radius) {
                return TEXT("Honeycomb wall thickness must be less than its cell radius.");
            }
            break;
        case ESbxMeshShape::BeveledBox:
            if (request.beveled_box.dimensions.GetMin() <= 0.0f ||
                request.beveled_box.bevel_width <= 0.0f) {
                return TEXT("Beveled-box dimensions and bevel width must be positive.");
            }
            if (request.beveled_box.bevel_width >= request.beveled_box.dimensions.GetMin() * 0.5f) {
                return TEXT("Beveled-box bevel width must be less than half its smallest "
                            "dimension.");
            }
            break;
        case ESbxMeshShape::Wedge:
            if (request.wedge.dimensions.GetMin() <= 0.0f || request.wedge.top_length <= 0.0f) {
                return TEXT("Wedge dimensions and top length must be positive.");
            }
            if (request.wedge.top_length > request.wedge.dimensions.X ||
                FMath::Abs(request.wedge.top_offset) + request.wedge.top_length * 0.5f >
                    request.wedge.dimensions.X * 0.5f) {
                return TEXT("Wedge top length and offset must keep the top within its base.");
            }
            break;
    }

    return {};
}

auto generate_mesh(FSbxMeshGenerationRequest const& request) -> FSbxMeshData {
    check(validate_mesh_request(request).IsEmpty());
    switch (request.shape) {
        case ESbxMeshShape::Box:
            return generate_box(request.box);
        case ESbxMeshShape::Cylinder:
            return generate_cylinder(request.cylinder);
        case ESbxMeshShape::Sphere:
            return generate_sphere(request.sphere);
        case ESbxMeshShape::Cone:
            return generate_cone(request.cone);
        case ESbxMeshShape::HexTile:
            return generate_hex_tile(request.hex_tile);
        case ESbxMeshShape::HexFrame:
            return generate_hex_frame(request.hex_frame);
        case ESbxMeshShape::HoneycombPanel:
            return generate_honeycomb_panel(request.honeycomb_panel);
        case ESbxMeshShape::BeveledBox:
            return generate_beveled_box(request.beveled_box);
        case ESbxMeshShape::Wedge:
            return generate_wedge(request.wedge);
    }

    checkNoEntry();
    return {};
}

auto describe_mesh_request(FSbxMeshGenerationRequest const& request) -> FString {
    switch (request.shape) {
        case ESbxMeshShape::Box:
            return FString::Printf(TEXT("version=1;shape=box;dimensions=%s"),
                                   *request.box.dimensions.ToString());
        case ESbxMeshShape::Cylinder:
            return FString::Printf(TEXT("version=1;shape=cylinder;radius=%g;height=%g;segments=%d"),
                                   request.cylinder.radius,
                                   request.cylinder.height,
                                   request.cylinder.radial_segments);
        case ESbxMeshShape::Sphere:
            return FString::Printf(
                TEXT("version=1;shape=sphere;radius=%g;longitude_segments=%d;latitude_segments=%d"),
                request.sphere.radius,
                request.sphere.longitude_segments,
                request.sphere.latitude_segments);
        case ESbxMeshShape::Cone:
            return FString::Printf(TEXT("version=1;shape=cone;radius=%g;height=%g;segments=%d"),
                                   request.cone.radius,
                                   request.cone.height,
                                   request.cone.radial_segments);
        case ESbxMeshShape::HexTile:
            return FString::Printf(TEXT("version=1;shape=hex_tile;outer_radius=%g;depth=%g;bevel_"
                                        "width=%g;pointy_top=%s"),
                                   request.hex_tile.outer_radius,
                                   request.hex_tile.depth,
                                   request.hex_tile.bevel_width,
                                   request.hex_tile.pointy_top ? TEXT("true") : TEXT("false"));
        case ESbxMeshShape::HexFrame:
            return FString::Printf(TEXT("version=1;shape=hex_frame;outer_radius=%g;wall_thickness=%"
                                        "g;depth=%g;pointy_top=%s"),
                                   request.hex_frame.outer_radius,
                                   request.hex_frame.wall_thickness,
                                   request.hex_frame.depth,
                                   request.hex_frame.pointy_top ? TEXT("true") : TEXT("false"));
        case ESbxMeshShape::HoneycombPanel:
            return FString::Printf(TEXT("version=1;shape=honeycomb_panel;rows=%d;columns=%d;cell_"
                                        "radius=%g;wall_thickness=%g;depth=%g;pointy_top=%s"),
                                   request.honeycomb_panel.rows,
                                   request.honeycomb_panel.columns,
                                   request.honeycomb_panel.cell_radius,
                                   request.honeycomb_panel.wall_thickness,
                                   request.honeycomb_panel.depth,
                                   request.honeycomb_panel.pointy_top ? TEXT("true")
                                                                      : TEXT("false"));
        case ESbxMeshShape::BeveledBox:
            return FString::Printf(TEXT("version=1;shape=beveled_box;dimensions=%s;bevel_width=%g"),
                                   *request.beveled_box.dimensions.ToString(),
                                   request.beveled_box.bevel_width);
        case ESbxMeshShape::Wedge:
            return FString::Printf(
                TEXT("version=1;shape=wedge;dimensions=%s;top_length=%g;top_offset=%g"),
                *request.wedge.dimensions.ToString(),
                request.wedge.top_length,
                request.wedge.top_offset);
    }

    checkNoEntry();
    return {};
}

}
