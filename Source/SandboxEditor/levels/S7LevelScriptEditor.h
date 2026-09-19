#pragma once

#include <CoreMinimal.h>
#include <Widgets/SCompoundWidget.h>

class SMultiLineEditableTextBox;
class STextBlock;
class US7LevelAuthoringMode;

class SS7LevelScriptEditor final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SS7LevelScriptEditor) {}
    SLATE_END_ARGS()

    ~SS7LevelScriptEditor() override;

    void Construct(FArguments const& arguments);
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
    void release_mode();
  private:
    void bind_active_mode();
    void refresh();
    void on_mode_changed();
    void on_source_changed(FText const& source);
    [[nodiscard]] auto active_mode() const -> US7LevelAuthoringMode*;
    [[nodiscard]] auto state_text() const -> FText;
    [[nodiscard]] auto can_load_source() const -> bool;
    [[nodiscard]] auto can_reload_source() const -> bool;
    [[nodiscard]] auto can_preview_buffer() const -> bool;
    [[nodiscard]] auto can_apply_preview() const -> bool;
    [[nodiscard]] auto can_save_buffer() const -> bool;
    [[nodiscard]] auto can_save_canonical() const -> bool;
    auto load_source() -> FReply;
    auto reload_source() -> FReply;
    auto preview_buffer() -> FReply;
    auto apply_preview() -> FReply;
    auto save_buffer() -> FReply;
    auto save_buffer_as() -> FReply;
    auto save_canonical() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    FDelegateHandle mode_changed_handle_{};
    TSharedPtr<SMultiLineEditableTextBox> source_{};
    TSharedPtr<STextBlock> state_{};
    TSharedPtr<STextBlock> status_{};
    float refresh_elapsed_seconds_{};
};
