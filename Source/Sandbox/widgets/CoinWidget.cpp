#include "Sandbox/widgets/CoinWidget.h"

void UCoinWidget::NativeConstruct() {
    Super::NativeConstruct();
    update_coin(0);
}

void UCoinWidget::update_coin(int32 coins) {
    static auto const fmt{FText::FromStringView(TEXT("Coins: {0}"))};

    if (coin_text) {
        auto const coin_display{FText::Format(fmt, FText::AsNumber(coins))};
        coin_text->SetText(coin_display);
    }
}
