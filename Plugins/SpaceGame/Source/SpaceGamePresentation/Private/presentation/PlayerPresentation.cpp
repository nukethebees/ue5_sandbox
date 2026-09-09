#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <NiagaraComponent.h>
#include <SpaceGamePresentation/presentation/PlayerPresentation.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

FPlayerPresentation::FPlayerPresentation(FPlayerPresentationResources resources,
                                         FPlayerShipConfig const& config,
                                         FPlayerReadView const& initial_state)
    : resources_{MoveTemp(resources)}
    , config_{config}
    , boost_start_sequence_{initial_state.boost_start_sequence} {
    if (!resources_.pulse.IsValid() || !resources_.engine.IsValid() || !resources_.mesh.IsValid()) {
        UE_LOG(LogSandbox, Error, TEXT("Player visual resources are incomplete"));
        return;
    }
    resources_.pulse->SetColorParameter(TEXT("colour"), config.engine_colour);
    resources_.pulse->SetFloatParameter(TEXT("ring_colour_intensity"),
                                        config.boost_effect_colour_intensity);
    resources_.pulse->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                        config.boost_effect_colour_intensity);
    resources_.engine->SetColorParameter(TEXT("colour"), config.engine_colour);
    resources_.engine->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                         config.boost_effect_colour_intensity);
    tick(initial_state);
}
void FPlayerPresentation::tick(FPlayerReadView const& state) {
    if (!resources_.root.IsValid() || !resources_.mesh.IsValid() || !resources_.pulse.IsValid() ||
        !resources_.engine.IsValid()) {
        return;
    }
    resources_.root->SetWorldTransform(
        state.transform, false, nullptr, ETeleportType::TeleportPhysics);
    resources_.mesh->SetRelativeTransform(state.body_transform);
    resources_.engine->SetVectorParameter(TEXT("ship_velocity"), state.velocity);
    if (boost_start_sequence_ != state.boost_start_sequence) {
        resources_.pulse->Activate();
        boost_start_sequence_ = state.boost_start_sequence;
    }
    if (boost_brake_state_ != state.boost_brake_state) {
        if (state.boost_brake_state == EBoostBrakeState::Boost) {
            resources_.engine->Activate();
        } else {
            resources_.engine->Deactivate();
        }
        boost_brake_state_ = state.boost_brake_state;
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
        draw_direction(state.middle_socket, 5000.f);
    }
    if (resources_.debug_forward_direction) {
        draw_direction(state.transform, 5000.f);
    }
    if (resources_.debug_lock_on &&
        state.laser_firing_mode == ELaserFiringState::lock_on_searching) {
        auto const end{draw_direction(state.middle_socket, config_.laser_lock_on_distance)};
        DrawDebugSphere(world, end, resources_.debug_lock_on_sphere_radius, 8, FColor::Orange);
    }
#endif
}
