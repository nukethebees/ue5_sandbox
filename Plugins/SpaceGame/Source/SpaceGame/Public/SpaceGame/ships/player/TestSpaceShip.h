#pragma once
#include <ioj/sim/entities/ship_health.h>
#include <ioj/vector.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/ShipHealth.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/PlayerPresentation.h>
#include <SpaceGamePresentation/support/logging/ActorLoggingConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Pawn.h>

#include <optional>

#include "TestSpaceShip.generated.h"

class UCameraComponent;
class UNiagaraComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace ioj {
class USpaceDustComponent;
class ATestBatchOrchestrator;

DECLARE_DELEGATE(FOnPlayerShipDied);

UCLASS()
class SPACEGAME_API ATestSpaceShip : public APawn {
    GENERATED_BODY()
    friend class ATestBatchOrchestrator;
  public:
    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    /* **************************************** */
    // Lifecycle and simulation binding
    /* **************************************** */
    ATestSpaceShip();
    void present_player(Team player_team, sim::ShipHealth health, Vector3d velocity);
    void clear_player_frame();

    /* **************************************** */
    // Entity identity and configuration
    /* **************************************** */
    auto get_team() const noexcept -> ETestTeam;
    void set_team(ETestTeam new_team) noexcept;

    /* **************************************** */
    // Health and collision
    /* **************************************** */
    auto get_health_info() const -> FShipHealth;
    auto is_alive() const noexcept -> bool;

    auto get_collision_mesh() const -> UStaticMesh const*;

    FOnPlayerShipDied on_player_ship_died;

    /* **************************************** */
    // Presentation
    /* **************************************** */
    auto get_presentation_resources() const -> FPlayerPresentationResources;

#if !UE_BUILD_SHIPPING
    void apply_space_dust_debug_preset(FStringView preset);
#endif

  private:
    /* **************************************** */
    // Simulation access and presentation
    /* **************************************** */
    auto GetVelocity() const -> FVector override;

    void handle_simulation_death();
    void configure_ship_mesh();

    /* **************************************** */
    // State
    /* **************************************** */

    UPROPERTY(EditAnywhere, Category = "Level Authoring")
    ETestTeam team{ETestTeam::White};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UCameraComponent* camera{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation", meta = (AllowPrivateAccess))
    USpaceDustComponent* space_dust{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_engine_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Logging", meta = (AllowPrivateAccess))
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_socket_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_direction{false};
#endif

    struct PlayerSnapshot {
        FShipHealth health;
        FVector velocity;
    };
    std::optional<PlayerSnapshot> player_snapshot_;

  public:
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditInstanceOnly,
              Category = "Level Authoring",
              meta = (DisplayName = "Export", ToolTip = "Save this entity to editor-config.scm."))
    bool export_to_script{true};
#endif

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "1"))
    TOptional<int32> health_override;
};

} // namespace ioj
