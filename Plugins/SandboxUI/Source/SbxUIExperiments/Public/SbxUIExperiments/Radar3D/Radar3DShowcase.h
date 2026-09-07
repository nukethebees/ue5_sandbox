#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "SandboxUI/Radar/RadarFrameStore.h"

#include "Radar3DShowcase.generated.h"

class SMultiLineEditableTextBox;
class SRadarWidget;

namespace SlateGenerated {
struct URadar3DShowcaseBuilder;
}

UCLASS(Blueprintable)
class SBXUIEXPERIMENTS_API URadar3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::URadar3DShowcaseBuilder;
  public:
    URadar3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;
    void set_contact_count(int32 contact_count);
    void populate_frame(int32 contact_count);

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
    TSharedPtr<SRadarWidget> radar_widget_;
    FRadarFrameStorePtr radar_frame_store_;
};
