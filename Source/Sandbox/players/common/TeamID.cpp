#include "Sandbox/players/common/TeamID.h"

#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"

auto get_team_attitude(ETeamID team_a, AActor& target) -> ETeamAttitude::Type {
    auto const* team_agent{Cast<IGenericTeamAgentInterface>(&target)};

    if (team_agent) {
        auto const team_id{team_agent->GetGenericTeamId()};
        return get_team_attitude(team_a, static_cast<ETeamID>(team_id.GetId()));
    }

    return ETeamAttitude::Neutral;
}
