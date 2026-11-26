#include <SpaceGamePresentation/presentation/PlayerPresentation.h>

#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <NiagaraComponent.h>

namespace ioj {
FPlayerPresentation::FPlayerPresentation(FPlayerPresentationResources resources,
                                         FPlayerShipConfig const& config,
                                         settings::PlayerPresentation const& settings,
                                         presentation::MissionFrame const& frame)
    : resources_{MoveTemp(resources)}
    , last_tick_{frame.status.completed_tick} {
    if (!resources_.pulse.IsValid() || !resources_.engine.IsValid() || !resources_.mesh.IsValid() ||
        !resources_.space_dust.IsValid()) {
        UE_LOG(LogSandbox, Error, TEXT("Player visual resources are incomplete"));
        return;
    }
    resources_.pulse->SetColorParameter(TEXT("colour"), config.engine_colour);
    resources_.pulse->SetFloatParameter(TEXT("ring_colour_intensity"),
                                        settings.boost_effect_colour_intensity);
    resources_.pulse->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                        settings.boost_effect_colour_intensity);
    resources_.engine->SetColorParameter(TEXT("colour"), config.engine_colour);
    resources_.engine->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                         settings.boost_effect_colour_intensity);
    resources_.space_dust->apply_settings(config.space_dust);
    tick(frame);
}
void FPlayerPresentation::tick(presentation::MissionFrame const& frame) {
    auto const& state{*frame.player};
    auto const action{state.flight.boost_brake_state};
    if (!resources_.root.IsValid() || !resources_.mesh.IsValid() || !resources_.pulse.IsValid() ||
        !resources_.engine.IsValid() || !resources_.space_dust.IsValid()) {
        return;
    }
    resources_.root->SetWorldTransform(
        to_unreal(state.transform), false, nullptr, ETeleportType::TeleportPhysics);
    resources_.mesh->SetRelativeTransform(to_unreal(state.body_transform));
    resources_.engine->SetVectorParameter(TEXT("ship_velocity"),
                                          to_unreal(state.flight.ship_velocity));
    resources_.space_dust->update_motion(to_unreal(state.flight.ship_velocity));
    auto const tick{frame.status.completed_tick};
    if (last_tick_ != tick) {
        for (auto const event : frame.events.player) {
            if (event == sim::player::PlayerEventKind::BoostStarted) {
                resources_.pulse->Activate();
                break;
            }
        }
        last_tick_ = tick;
    }
    if (boost_brake_state_ != action) {
        if (action == sim::player::BoostBrakeState::Boost) {
            resources_.engine->Activate();
        } else {
            resources_.engine->Deactivate();
        }
        boost_brake_state_ = action;
    }
#if WITH_EDITORONLY_DATA
    auto* const world{resources_.root->GetWorld()};
    auto draw_direction = [world](FTransform const& transform, float distance) {
        auto const start{transform.GetLocation()};
        auto const end{start + transform.GetUnitAxis(EAxis::X) * distance};
        DrawDebugLine(world, start, end, FColor::Green, false, 0.f, 0, 10.f);
        return end;
    };
    if (resources_.debug_forward_socket_direction) {
        draw_direction(to_unreal(state.middle_socket), 5000.f);
    }
    if (resources_.debug_forward_direction) {
        draw_direction(to_unreal(state.transform), 5000.f);
    }
#endif
}

} // namespace ioj
