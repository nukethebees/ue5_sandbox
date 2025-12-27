#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/health/interfaces/DeathHandler.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/npcs/enums/AIState.h"
#include "Sandbox/players/npcs/enums/MobAttackMode.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"

#include "SimpleCharacter.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

class UHealthComponent;

UCLASS()
class SANDBOX_API ASimpleCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"ASimpleCharacter", LogSandboxCharacter>
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface {
    GENERATED_BODY()
  public:
    ASimpleCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    EMobAttackMode attack_mode{EMobAttackMode::None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    FLinearColor light_colour{FLinearColor::White};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    ETeamID team_id{ETeamID::Enemy};

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ISandboxMobInterface
    virtual float get_acceptable_radius() const override;
    virtual EAIState get_default_ai_state() const override { return default_ai_state; }
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    float acceptable_radius{100.0f};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    UPointLightComponent* light{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    UHealthComponent* health{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    EAIState default_ai_state{EAIState::RandomlyMove};
  private:
    // IDeathHandler
    virtual void handle_death() override;

    void apply_material_colours();
    void apply_light_colours();

    UMaterialInstanceDynamic* dynamic_material{nullptr};
};
