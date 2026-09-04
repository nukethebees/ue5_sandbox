#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SandboxNiagaraShowcaseActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UTextRenderComponent;

USTRUCT()
struct FSandboxNiagaraShowcaseEntry {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    TObjectPtr<UNiagaraSystem> system{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    FString display_name{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    FTransform effect_transform{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    FTransform label_transform{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    bool replay_burst{false};
};

UCLASS()
class SANDBOXNIAGARAEDITOR_API ASandboxNiagaraShowcaseActor : public AActor {
    GENERATED_BODY()

  public:
    ASandboxNiagaraShowcaseActor();

    void configure(TArray<FSandboxNiagaraShowcaseEntry> entries, float replay_interval);

    void Tick(float delta_seconds) override;
    auto ShouldTickIfViewportsOnly() const -> bool override;
    void OnConstruction(FTransform const& transform) override;
    void PostRegisterAllComponents() override;

  private:
    void rebuild_components();

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    TObjectPtr<USceneComponent> root_component_{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    TArray<FSandboxNiagaraShowcaseEntry> entries_{};

    UPROPERTY(VisibleAnywhere, Instanced, Category = "Sandbox Niagara")
    TArray<TObjectPtr<UNiagaraComponent>> effect_components_{};

    UPROPERTY(VisibleAnywhere, Instanced, Category = "Sandbox Niagara")
    TArray<TObjectPtr<UTextRenderComponent>> label_components_{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox Niagara")
    float replay_interval_{2.0f};

    float replay_elapsed_{0.0f};
};
