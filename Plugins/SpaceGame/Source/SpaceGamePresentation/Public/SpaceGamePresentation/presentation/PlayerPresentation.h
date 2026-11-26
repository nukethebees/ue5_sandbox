#pragma once
#include <ioj/presentation/mission_frame.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>

#include <UObject/WeakObjectPtrTemplates.h>

class USceneComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
namespace ioj {
class USpaceDustComponent;

struct FPlayerPresentationResources {
    TWeakObjectPtr<USceneComponent> root;
    TWeakObjectPtr<UStaticMeshComponent> mesh;
    TWeakObjectPtr<UNiagaraComponent> pulse;
    TWeakObjectPtr<UNiagaraComponent> engine;
    TWeakObjectPtr<USpaceDustComponent> space_dust;
    bool debug_forward_socket_direction{};
    bool debug_forward_direction{};
};
struct SPACEGAMEPRESENTATION_API FPlayerPresentation {
    FPlayerPresentation(FPlayerPresentationResources resources,
                        FPlayerShipConfig const& config,
                        settings::PlayerPresentation const& settings,
                        presentation::MissionFrame const& frame);
    void tick(presentation::MissionFrame const& frame);
  private:
    FPlayerPresentationResources resources_;
    sim::player::BoostBrakeState boost_brake_state_{sim::player::BoostBrakeState::None};
    sim::SimTick last_tick_{};
};

} // namespace ioj
