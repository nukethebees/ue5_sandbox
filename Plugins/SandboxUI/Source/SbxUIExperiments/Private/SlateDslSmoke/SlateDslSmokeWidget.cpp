#include "SbxUIExperiments/SlateDslSmoke/SlateDslSmokeWidget.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <utility>

#include "generated/USlateDslSmokeWidget.slate.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlateDslSmokeWidget, Log, All);

USlateDslSmokeWidget::USlateDslSmokeWidget() {
    TabDisplayName = NSLOCTEXT("SlateDslSmoke", "TabName", "Slate DSL Smoke Widget");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> USlateDslSmokeWidget::RebuildWidget() {
    auto on_value_changed{[this](int32 const value) {
        selected_value_ = value;
        UE_LOG(LogSlateDslSmokeWidget, Display, TEXT("Generated slider value: %d"), value);
    }};

    return SlateGenerated::USlateDslSmokeWidgetBuilder{*this}.RebuildWidget(
        std::move(on_value_changed));
}

auto USlateDslSmokeWidget::handle_clicked() -> FReply {
    ++click_count_;
    UE_LOG(LogSlateDslSmokeWidget,
           Display,
           TEXT("Generated button clicked %d time(s); selected value is %d"),
           click_count_,
           selected_value_);
    return FReply::Handled();
}
