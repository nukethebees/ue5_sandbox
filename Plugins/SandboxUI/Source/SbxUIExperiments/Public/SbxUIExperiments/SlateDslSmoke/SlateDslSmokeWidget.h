#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "SlateDslSmokeWidget.generated.h"

namespace SlateGenerated {
struct USlateDslSmokeWidgetBuilder;
}

UCLASS(Blueprintable)
class SBXUIEXPERIMENTS_API USlateDslSmokeWidget : public UEditorUtilityWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::USlateDslSmokeWidgetBuilder;
  public:
    USlateDslSmokeWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto handle_clicked() -> FReply;

    int32 selected_value_{5};
    int32 click_count_{};
};
