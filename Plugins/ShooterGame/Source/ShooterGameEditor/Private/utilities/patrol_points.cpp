#include "ShooterGameEditor/utilities/patrol_points.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelEditor.h"
#include "Selection.h"

#include "ShooterGame/pathfinding/PatrolPath.h"
#include "ShooterGame/pathfinding/PatrolWaypoint.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

namespace ml {
void add_selected_patrol_points_to_selected_paths() {
    auto* selected_actors{GEditor->GetSelectedActors()};

    TArray<APatrolPath*> selected_paths;
    selected_actors->GetSelectedObjects<APatrolPath>(selected_paths);
    TArray<APatrolWaypoint*> selected_points;
    selected_actors->GetSelectedObjects<APatrolWaypoint>(selected_points);

    for (auto* path : selected_paths) {
        if (!path) {
            WARN_IS_FALSE(LogTemp, path);
            continue;
        }

        for (auto* point : selected_points) {
            if (!point) {
                WARN_IS_FALSE(LogTemp, point);
                continue;
            }

            path->add_point(*point);
        }
    }
}
}
