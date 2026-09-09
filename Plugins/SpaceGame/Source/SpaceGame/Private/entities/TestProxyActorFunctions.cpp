#include "SpaceGame/entities/TestProxyActorFunctions.h"

#include "SpaceGameSimulation/entities/TestTeam.h"

#include <GameFramework/Actor.h>
#if WITH_EDITOR
#include <Editor/EditorEngine.h>
#include <EngineUtils.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#endif

namespace ml {
#if WITH_EDITOR
auto resolve_proxy_level_config(AActor const& actor) -> USpaceGameLevelConfig* {
    auto* const world{actor.GetWorld()};
    if (!IsValid(world) || world->WorldType != EWorldType::Editor) {
        return nullptr;
    }
    ATestBatchOrchestrator* orchestrator{};
    for (TActorIterator<ATestBatchOrchestrator> it{world}; it; ++it) {
        if (orchestrator) {
            return nullptr;
        }
        orchestrator = *it;
    }
    return IsValid(orchestrator) ? orchestrator->get_level_config() : nullptr;
}
#endif

void set_proxy_actor_name(AActor& actor, FString const& type, ETestTeam const team) {
#if WITH_EDITOR
    auto const* const team_enum{StaticEnum<ETestTeam>()};
    auto const team_name{team_enum->GetNameStringByValue(static_cast<int64>(team))};
    auto const label{FString::Printf(TEXT("%s_%s"), *type, *team_name)};
    FActorLabelUtilities::SetActorLabelUnique(&actor, label);
#else
    static_cast<void>(actor);
    static_cast<void>(type);
    static_cast<void>(team);
#endif
}
}
