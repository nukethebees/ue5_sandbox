#include "SpaceGame/entities/TestProxyActorFunctions.h"

#include "SpaceGame/entities/TestTeam.h"

#include <GameFramework/Actor.h>
#if WITH_EDITOR
#include <Editor/EditorEngine.h>
#endif

namespace ml {
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
