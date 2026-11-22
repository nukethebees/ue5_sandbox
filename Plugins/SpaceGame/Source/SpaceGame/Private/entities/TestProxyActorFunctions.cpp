#include "SpaceGame/entities/TestProxyActorFunctions.h"

#include "SpaceGamePresentation/entities/TestTeam.h"

#include <GameFramework/Actor.h>
#if WITH_EDITOR
#include <Editor/EditorEngine.h>
#endif

namespace ioj {
#if WITH_EDITOR
namespace {
auto generated_label_suffix(FString const& label, FString const& type) -> TOptional<FString> {
    int32 end{label.Len()};
    while (end > 0 && FChar::IsDigit(label[end - 1])) {
        --end;
    }
    auto stem{label.Left(end).Replace(TEXT("-"), TEXT("")).Replace(TEXT("_"), TEXT(""))};
    if (!stem.RemoveFromStart(type, ESearchCase::IgnoreCase)) {
        return NullOpt;
    }
    if (!stem.IsEmpty() && StaticEnum<ETestTeam>()->GetIndexByNameString(stem) == INDEX_NONE) {
        return NullOpt;
    }
    return label.Mid(end);
}
}
#endif

void set_proxy_actor_name(AActor& actor,
                          FString const& type,
                          ETestTeam const team,
                          ProxyLabelUpdate const update) {
#if WITH_EDITOR
    auto const* const team_enum{StaticEnum<ETestTeam>()};
    auto const team_name{team_enum->GetNameStringByValue(static_cast<int64>(team))};
    auto label{FString::Printf(TEXT("%s_%s"), *type, *team_name)};
    if (update == ProxyLabelUpdate::Refresh) {
        auto const& current{actor.GetActorLabel()};
        auto const suffix{generated_label_suffix(current, type)};
        if (!suffix) {
            return;
        }
        label += *suffix;
        if (current.Equals(label, ESearchCase::CaseSensitive)) {
            return;
        }
    }
    FActorLabelUtilities::SetActorLabelUnique(&actor, label);
#else
    static_cast<void>(actor);
    static_cast<void>(type);
    static_cast<void>(team);
    static_cast<void>(update);
#endif
}
}
