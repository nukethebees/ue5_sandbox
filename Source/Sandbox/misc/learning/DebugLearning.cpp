#include "DebugLearning.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>

ADebugLearning::ADebugLearning()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Life cycle
void ADebugLearning::BeginPlay() {
    Super::BeginPlay();
}
void ADebugLearning::Tick(float dt) {
    Super::Tick(dt);

    auto* world{GetWorld()};
    auto const pos{GetActorLocation()};
    auto const fwd{GetActorForwardVector()};
    auto const right{GetActorRightVector()};

    auto end{pos + fwd * line_length};

    auto start{pos};
    auto move_coords{[&] {
        auto const delta_pos{right * spacing};
        start += delta_pos;
        end += delta_pos;
    }};

    auto get_transform{[&] {
        auto transform{FTransform::Identity};
        transform.SetLocation(start);
        return transform;
    }};

    constexpr bool persistent{false};
    constexpr float lifetime{0.f};
    constexpr auto depth_priority{0u};

    DrawDebugLine(world, start, end, colour, persistent, lifetime, depth_priority, line_thickness);

    move_coords();
    DrawDebugPoint(world, start, point_size, colour);

    move_coords();
    DrawDebugDirectionalArrow(world,
                              start,
                              end,
                              arrow_size,
                              arrow_colour,
                              persistent,
                              lifetime,
                              depth_priority,
                              line_thickness);

    auto const box_side{line_length / 2.f};
    FVector const box_extent{box_side};

    move_coords();
    DrawDebugBox(
        world, start, box_extent, box_colour, persistent, lifetime, depth_priority, line_thickness);

    move_coords();
    DrawDebugCoordinateSystem(world,
                              start,
                              FRotator::ZeroRotator,
                              coord_scale,
                              persistent,
                              lifetime,
                              depth_priority,
                              line_thickness);

    // DrawDebugCrosshairs

    move_coords();

    auto circle_transform{get_transform()};
    DrawDebugCircle(world,
                    circle_transform.ToMatrixWithScale(),
                    radius,
                    segments,
                    colour,
                    persistent,
                    lifetime,
                    depth_priority,
                    line_thickness,
                    draw_axis);

    move_coords();
    DrawDebugCircleArc(world,
                       start,
                       radius,
                       FQuat::Identity,
                       FMath::DegreesToRadians(circle_arc_angle / 2.f),
                       segments,
                       circle_arc_colour,
                       persistent,
                       lifetime,
                       depth_priority,
                       thickness);

    move_coords();
    auto donut_transform{get_transform()};

    DrawDebug2DDonut(world,
                     donut_transform.ToMatrixWithScale(),
                     donut_inner_radius,
                     donut_outer_radius,
                     donut_segments,
                     donut_colour,
                     persistent,
                     lifetime,
                     depth_priority,
                     line_thickness);

    auto const sphere_radius{line_length / 2.f};

    move_coords();
    DrawDebugSphere(world,
                    start,
                    sphere_radius,
                    sphere_segments,
                    sphere_colour,
                    persistent,
                    lifetime,
                    depth_priority,
                    line_thickness);

    move_coords();
    DrawDebugCylinder(world,
                      start,
                      end,
                      radius,
                      segments,
                      colour,
                      persistent,
                      lifetime,
                      depth_priority,
                      thickness);

    move_coords();
    DrawDebugCone(world,
                  start,
                  fwd,
                  length,
                  FMath::DegreesToRadians(cone_angle_width / 2.f),
                  FMath::DegreesToRadians(cone_angle_height / 2.f),
                  segments,
                  cone_colour,
                  persistent,
                  lifetime,
                  depth_priority,
                  thickness);

    // DrawDebugAltCone

    // DrawDebugString

    // DrawDebugFrustum

    // DrawCircle

    move_coords();
    DrawDebugCapsule(world,
                     start,
                     capsule_half_height,
                     capsule_radius,
                     FQuat::Identity,
                     capsule_colour,
                     persistent,
                     lifetime,
                     depth_priority,
                     thickness);

    // DrawDebugCamera

    // DrawCentripetalCatmullRomSpline

    // DrawDebugSolidBox

    // DrawDebugMesh

    // DrawDebugSolidPlane

    // DrawDebugFloatHistory
}
