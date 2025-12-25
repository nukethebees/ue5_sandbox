// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolPath/PatrolPath.h"

#include "DrawDebugHelpers.h"

#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

APatrolPath::APatrolPath() {
#if WITH_EDITOR
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
#else
    PrimaryActorTick.bCanEverTick = false;
#endif

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void APatrolPath::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    auto const elems{length()};
    if (IsSelected() && (elems >= 2)) {
        TRY_INIT_PTR(world, GetWorld());
        for (int32 i{1}; i < elems; ++i) {
            draw_line_between_waypoints(*world, i - 1, i);
        }
        draw_line_between_waypoints(*world, elems - 1, 0);
    }
#endif
}

#if WITH_EDITOR
void APatrolPath::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    FName property_name{event.Property ? event.Property->GetFName() : NAME_None};
    if (property_name == GET_MEMBER_NAME_CHECKED(APatrolPath, name)) {
        SetActorLabel(name.ToString());
    }
}

void APatrolPath::draw_line_between_waypoints(UWorld& world, int32 a, int32 b) {
    auto const start{waypoints[a]->GetActorLocation()};
    auto const end{waypoints[b]->GetActorLocation()};

    constexpr bool persistent{false};
    constexpr float lifetime{-1.0f};
    constexpr int32 depth_priority{0};

    DrawDebugDirectionalArrow(&world,
                              start,
                              end,
                              editor_line_arrow_size,
                              debug_line_color,
                              persistent,
                              lifetime,
                              depth_priority,
                              editor_line_thickness);
}

#endif
