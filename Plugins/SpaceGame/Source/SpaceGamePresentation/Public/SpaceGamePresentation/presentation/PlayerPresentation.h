#pragma once
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGameSimulation/ships/player/PlayerReadView.h>
#include <UObject/WeakObjectPtrTemplates.h>

class USceneComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
struct FPlayerPresentationResources {
    TWeakObjectPtr<USceneComponent> root;
    TWeakObjectPtr<UStaticMeshComponent> mesh;
    TWeakObjectPtr<UNiagaraComponent> pulse;
    TWeakObjectPtr<UNiagaraComponent> engine;
    bool debug_forward_socket_direction{};
    bool debug_forward_direction{};
    bool debug_lock_on{};
    float debug_lock_on_sphere_radius{1000.f};
};
struct SPACEGAMEPRESENTATION_API FPlayerPresentation {
    FPlayerPresentation(FPlayerPresentationResources resources,
                        FPlayerShipConfig const& config,
                        FPlayerReadView const& initial_state);
    void tick(FPlayerReadView const& state);
  private:
    FPlayerPresentationResources resources_;
    FPlayerShipConfig config_;
    EBoostBrakeState boost_brake_state_{EBoostBrakeState::None};
    uint64 boost_start_sequence_{};
};
