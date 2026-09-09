#pragma once
#include <SpaceGamePresentation/ui/style/SpaceGameUiTheme.h>
#include <Subsystems/GameInstanceSubsystem.h>
#include "GameUiStyleSubsystem.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAMEPRESENTATION_API UGameUiStyleSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    void Initialize(FSubsystemCollectionBase& collection) override;
    auto get_ui_style() const -> FGameUiStyle const& { return style_; }
    auto set_ui_theme(USpaceGameUiTheme* theme) -> bool;
  private:
    UPROPERTY()
    TObjectPtr<USpaceGameUiTheme> theme_;
    FGameUiStyle style_;
};
}
