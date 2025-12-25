// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolPath/PatrolPath.h"

#include "DrawDebugHelpers.h"

#include "Sandbox/core/SandboxDeveloperSettings.h"
#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

#if WITH_EDITOR
int32 APatrolPath::color_index{0};
#endif

APatrolPath::APatrolPath() {
#if WITH_EDITOR
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    static TArray<FColor> const palette{
        {FColor::Blue, FColor::Red, FColor::Green, FColor::Cyan, FColor::Magenta, FColor::Yellow}};
    auto const i{color_index++ % palette.Num()};
    editor_line_color = palette[i];
#else
    PrimaryActorTick.bCanEverTick = false;
#endif

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void APatrolPath::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    auto const elems{length()};
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    auto const render_lines{settings->visualise_all_patrol_paths ||
                            !editor_line_require_selection || IsSelected()};

    if (render_lines && (elems >= 2) && !any_points_null()) {
        TRY_INIT_PTR(world, GetWorld());
        for (int32 i{1}; i < elems; ++i) {
            draw_line_between_waypoints(*world, i - 1, i);
        }
        draw_line_between_waypoints(*world, elems - 1, 0);
    }
#endif
}
bool APatrolPath::any_points_null() const {
    for (auto const* ptr : waypoints) {
        if (!ptr) {
            return true;
        }
    }

    return false;
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
                              editor_line_color,
                              persistent,
                              lifetime,
                              depth_priority,
                              editor_line_thickness);
}

#endif
