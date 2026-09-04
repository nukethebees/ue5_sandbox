#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxNiagaraSubsystem.generated.h"

class UNiagaraSystem;

UENUM(BlueprintType)
enum class ESandboxNiagaraEmissionMode : uint8 {
    Continuous,
    Burst,
};

UENUM(BlueprintType)
enum class ESandboxNiagaraSpawnShape : uint8 {
    Sphere,
    Disc,
    Cylinder,
    Ring,
};

USTRUCT(BlueprintType)
struct FSandboxNiagaraFloatParameter {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    FName name{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float value{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    FString display_name{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float minimum{-1000000000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float maximum{1000000000.0f};
};

USTRUCT(BlueprintType)
struct FSandboxNiagaraExperimentConfiguration {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    FString output_path{TEXT("/SandboxNiagara/Generated")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    ESandboxNiagaraEmissionMode emission_mode{ESandboxNiagaraEmissionMode::Continuous};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_rate{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    int32 burst_count{64};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    bool local_space{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float particle_lifetime{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float sprite_size{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    ESandboxNiagaraSpawnShape spawn_shape{ESandboxNiagaraSpawnShape::Sphere};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_radius{250.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_height{500.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float spawn_inner_radius{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    float fixed_bounds_extent{750.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    FLinearColor particle_color{FLinearColor::White};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara", meta = (MultiLine))
    FString particle_velocity_expression{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox Niagara")
    TArray<FSandboxNiagaraFloatParameter> float_parameters{};
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

USTRUCT(BlueprintType)
struct FSandboxNiagaraExperimentDefinition {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString id{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString display_name{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString default_asset_name{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FString source_file{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    FSandboxNiagaraExperimentConfiguration configuration{};
};

USTRUCT(BlueprintType)
struct FSandboxNiagaraExperimentCatalogResult {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    bool success{false};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox Niagara")
    TArray<FSandboxNiagaraExperimentDefinition> experiments{};

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
    FSandboxNiagaraExperimentCatalogResult load_experiment_catalog() const;

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult validate_template(UNiagaraSystem* template_system);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraGenerationResult
    generate_experiment(UNiagaraSystem* template_system,
                        FString const& experiment_name,
                        FSandboxNiagaraExperimentConfiguration const& configuration);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraGenerationResult
    regenerate_experiment(UNiagaraSystem* template_system,
                          FString const& experiment_name,
                          FSandboxNiagaraExperimentConfiguration const& configuration);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult delete_generated_asset(FString const& asset_path);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult regenerate_all_experiments(UNiagaraSystem* template_system);

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult create_or_refresh_showcase();

    UFUNCTION(BlueprintCallable, Category = "Sandbox Niagara")
    FSandboxNiagaraValidationResult open_showcase();

  private:
    FSandboxNiagaraGenerationResult
    generate_experiment_internal(UNiagaraSystem* template_system,
                                 FString const& experiment_name,
                                 FSandboxNiagaraExperimentConfiguration const& configuration,
                                 bool replace_existing);
};
