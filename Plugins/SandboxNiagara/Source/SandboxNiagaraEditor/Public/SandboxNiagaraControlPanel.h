#pragma once

#include "SandboxNiagaraSubsystem.h"

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "Types/SlateEnums.h"

#include "SandboxNiagaraControlPanel.generated.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
struct FAssetData;
template <typename OptionType>
class SComboBox;

UCLASS(Blueprintable)
class SANDBOXNIAGARAEDITOR_API USandboxNiagaraControlPanel : public UEditorUtilityWidget {
    GENERATED_BODY()

  public:
    USandboxNiagaraControlPanel();

  protected:
    TSharedRef<SWidget> RebuildWidget() override;

  private:
    void select_template(FAssetData const& asset_data);
    void select_preset(TSharedPtr<ESandboxNiagaraExperimentPreset> preset,
                       ESelectInfo::Type selection_type);
    auto generate() -> FReply;
    auto regenerate() -> FReply;
    auto regenerate_all() -> FReply;
    auto open_generated_asset() -> FReply;
    auto delete_generated_asset() -> FReply;
    auto run_generation(bool replace_existing) -> FReply;
    auto get_template_system() const -> UNiagaraSystem*;
    auto get_experiment_name() const -> FString;
    auto get_generated_asset_path() const -> FString;
    void set_status(FString const& message, bool success);
    void set_generation_status(FSandboxNiagaraGenerationResult const& result);
    void set_operation_status(FSandboxNiagaraValidationResult const& result,
                              FString const& success_message);

    FSoftObjectPath template_asset_path_{};
    ESandboxNiagaraExperimentPreset selected_preset_{
        ESandboxNiagaraExperimentPreset::Orbit};
    TArray<TSharedPtr<ESandboxNiagaraExperimentPreset>> preset_options_{};
    TSharedPtr<SEditableTextBox> experiment_name_input_{};
    TSharedPtr<SMultiLineEditableTextBox> status_output_{};
};
