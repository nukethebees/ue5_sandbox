#include "Sandbox/players/common/enums/TeamID.h"

#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"

auto get_team_attitude(ETeamID team_a, AActor& target) -> ETeamAttitude::Type {
    auto const* team_agent{Cast<IGenericTeamAgentInterface>(&target)};

    if (team_agent) {
        auto const team_id{team_agent->GetGenericTeamId()};
        return GetTeamAttitude(static_cast<FGenericTeamId>(std::to_underlying(team_a)), team_id);
    }

    return ETeamAttitude::Neutral;
}
