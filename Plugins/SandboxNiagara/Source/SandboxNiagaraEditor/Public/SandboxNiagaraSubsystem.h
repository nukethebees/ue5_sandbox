#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxNiagaraSubsystem.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FSandboxNiagaraExperimentConfiguration {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    FString output_path{TEXT("/SandboxNiagara/Generated")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_rate{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float particle_lifetime{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float sprite_size{4.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_radius{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float fixed_bounds_extent{5000.0f};
};

USTRUCT(BlueprintType)
struct FSandboxNiagaraValidationResult {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    bool success{false};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TArray<FString> warnings{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TArray<FString> errors{};
};

USTRUCT(BlueprintType)
struct FSandboxNiagaraGenerationResult {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    bool success{false};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TObjectPtr<UNiagaraSystem> generated_system{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString generated_asset_path{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString compile_status{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TArray<FString> warnings{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TArray<FString> errors{};
};

UCLASS()
class SANDBOXNIAGARAEDITOR_API USandboxNiagaraSubsystem : public UEditorSubsystem {
    GENERATED_BODY()

  public:
    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult validate_template(UNiagaraSystem* template_system);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraGenerationResult
    generate_experiment(UNiagaraSystem* template_system,
                        FString const& experiment_name,
                        FSandboxNiagaraExperimentConfiguration const& configuration);
};
