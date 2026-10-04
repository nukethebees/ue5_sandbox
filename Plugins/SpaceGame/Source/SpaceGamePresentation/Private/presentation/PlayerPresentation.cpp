#include <SpaceGamePresentation/presentation/PlayerPresentation.h>

#include <SpaceGamePresentation/integration/TransformConversion.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <NiagaraComponent.h>

FPlayerPresentation::FPlayerPresentation(FPlayerPresentationResources resources,
                                         FPlayerShipConfig const& config,
                                         ::ioj::sim::player::Sim const& initial_state)
    : resources_{MoveTemp(resources)}
    , config_{config}
    , boost_start_sequence_{initial_state.get_presentation_state().boost_start_sequence} {
    if (!resources_.pulse.IsValid() || !resources_.engine.IsValid() || !resources_.mesh.IsValid() ||
        !resources_.space_dust.IsValid()) {
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
    resources_.space_dust->apply_settings(config.space_dust);
    tick(initial_state);
}
void FPlayerPresentation::tick(::ioj::sim::player::Sim const& state) {
    auto const& physical{state.get_physical_state()};
    auto const& presentation{state.get_presentation_state()};
    auto const action{state.get_controller_state().effective_action};
    if (!resources_.root.IsValid() || !resources_.mesh.IsValid() || !resources_.pulse.IsValid() ||
        !resources_.engine.IsValid() || !resources_.space_dust.IsValid()) {
        return;
    }
    resources_.root->SetWorldTransform(
        ml::to_unreal(physical.transform), false, nullptr, ETeleportType::TeleportPhysics);
    resources_.mesh->SetRelativeTransform(ml::to_unreal(presentation.body_transform));
    resources_.engine->SetVectorParameter(TEXT("ship_velocity"), ml::to_unreal(physical.velocity));
    resources_.space_dust->update_motion(ml::to_unreal(physical.velocity));
    if (boost_start_sequence_ != presentation.boost_start_sequence) {
        resources_.pulse->Activate();
        boost_start_sequence_ = presentation.boost_start_sequence;
    }
    if (boost_brake_state_ != action) {
        if (action == ::ioj::sim::player::BoostBrakeState::Boost) {
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
        draw_direction(ml::to_unreal(state.get_middle_socket()), 5000.f);
    }
    if (resources_.debug_forward_direction) {
        draw_direction(ml::to_unreal(physical.transform), 5000.f);
    }
#endif
}
