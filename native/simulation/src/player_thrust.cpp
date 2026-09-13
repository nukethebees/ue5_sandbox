#include "sandbox/simulation/player_thrust.h"

namespace ml::simulation::player {
auto make_thrust_transition(ThrustSettings const settings,
                            BoostBrakeState const state,
                            float const current_speed) noexcept -> ThrustTransition {
    switch (state) {
        case BoostBrakeState::Boost: {
            return {settings.boost_speed,
                    -(1.f / settings.boost_depletion_time),
                    settings.cruise_speed * settings.boost_forward_speed_addition_multiplier,
                    SpeedResponseKind::Boost};
        }
        case BoostBrakeState::Brake: {
            return {settings.brake_speed,
                    -(1.f / settings.brake_depletion_time),
                    0.f,
                    SpeedResponseKind::Brake};
        }
        default: {
            return {settings.cruise_speed,
                    1.f / settings.thrust_recharge_time,
                    0.f,
                    settings.cruise_speed < current_speed
                        ? SpeedResponseKind::SlowingToCruise
                        : SpeedResponseKind::AcceleratingToCruise};
        }
    }
}
}
