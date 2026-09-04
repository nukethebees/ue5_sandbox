#pragma once

#include <CoreMinimal.h>

#include "PauseMenuControlContext.generated.h"

class ASpaceGamePlayerController;
class UTestBatchGameUiData;

namespace ml::ioj {
class UPauseMenuWidget;
}

USTRUCT()
struct SPACEGAME_API FPauseMenuControlContext {
    GENERATED_BODY()
  public:
    auto initialise(ASpaceGamePlayerController& owner, UTestBatchGameUiData& ui_data) -> bool;
    auto bind() -> bool;
    void unbind();
    void shutdown();

    [[nodiscard]] auto can_bind() const -> bool;
    [[nodiscard]] auto is_initialised() const noexcept -> bool { return initialised_; }
    [[nodiscard]] auto is_bound() const noexcept -> bool { return bound_; }
    [[nodiscard]] auto get_widget() const -> ml::ioj::UPauseMenuWidget* {
        return pause_menu_widget;
    }
  private:
    TWeakObjectPtr<ASpaceGamePlayerController> owner_;

    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::UPauseMenuWidget> pause_menu_widget{nullptr};

    bool initialised_{false};
    bool bound_{false};
};
