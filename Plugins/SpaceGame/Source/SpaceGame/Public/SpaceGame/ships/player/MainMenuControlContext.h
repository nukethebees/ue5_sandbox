#pragma once

#include <CoreMinimal.h>

#include "MainMenuControlContext.generated.h"

class ASpaceGamePlayerController;

namespace ml::ioj {
class UMainMenuWidget;
}

USTRUCT()
struct SPACEGAME_API FMainMenuControlContext {
    GENERATED_BODY()
  public:
    auto initialise(ASpaceGamePlayerController& owner,
                    TSubclassOf<ml::ioj::UMainMenuWidget> widget_class) -> bool;
    auto bind() -> bool;
    void unbind();
    void shutdown();

    [[nodiscard]] auto can_bind() const -> bool;
    [[nodiscard]] auto is_initialised() const noexcept -> bool { return initialised_; }
    [[nodiscard]] auto is_bound() const noexcept -> bool { return bound_; }
  private:
    void select_camera();

    TWeakObjectPtr<ASpaceGamePlayerController> owner_;

    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::UMainMenuWidget> widget_{nullptr};

    bool initialised_{false};
    bool bound_{false};
};
