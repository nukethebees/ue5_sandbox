#include "MyHUD.h"

#include "Sandbox/game_states/PlatformerGameState.h"
#include "UObject/ConstructorHelpers.h"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

#define MAIN_WIDGET_NULL_CHECK()                                          \
    do {                                                                  \
        if (!main_widget) {                                               \
            UE_LOGFMT(LogTemp, Error, "AMyHUD: main_widget not present"); \
            return;                                                       \
        }                                                                 \
    } while (0)

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (!main_widget_class) {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No main_widget_class."));
    }
    main_widget = CreateWidget<UMainHUDWidget>(GetWorld(), main_widget_class);
    if (main_widget) {
        main_widget->AddToViewport();
    }

    if (auto* game_state{GetWorld()->GetGameState<APlatformerGameState>()}) {
        game_state->on_coin_count_changed.AddDynamic(this, &AMyHUD::update_coin);
    }
}

void AMyHUD::update_fuel(float new_fuel) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_fuel(new_fuel);
}

void AMyHUD::update_jump(int32 new_jump) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_jump(new_jump);
}

void AMyHUD::update_coin(int32 new_coin_count) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_coin(new_coin_count);
}
