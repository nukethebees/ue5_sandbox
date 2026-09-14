#pragma once
#include <ioj/sim/player/player_read_view.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
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
                        ::ioj::sim::PlayerReadView const& initial_state);
    void tick(::ioj::sim::PlayerReadView const& state);
  private:
    FPlayerPresentationResources resources_;
    FPlayerShipConfig config_;
    ::ioj::sim::player::BoostBrakeState boost_brake_state_{
        ::ioj::sim::player::BoostBrakeState::None};
    uint64 boost_start_sequence_{};
};
