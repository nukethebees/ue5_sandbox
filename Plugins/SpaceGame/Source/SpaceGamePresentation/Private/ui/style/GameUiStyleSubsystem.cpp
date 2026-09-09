#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>
#include <SpaceGamePresentation/ui/style/SpaceGameUiSettings.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

namespace ml::ioj {
void UGameUiStyleSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    auto* theme{GetDefault<USpaceGameUiSettings>()->default_theme.LoadSynchronous()};
    if (!IsValid(theme)) {
        theme = NewObject<USpaceGameUiTheme>(this);
    }
    set_ui_theme(theme);
}
auto UGameUiStyleSubsystem::set_ui_theme(USpaceGameUiTheme* const theme) -> bool {
    if (!IsValid(theme)) {
        UE_LOG(LogSandboxUI, Error, TEXT("Cannot apply an invalid UI theme"));
        return false;
    }
    theme_ = theme;
    style_ = theme->compile();
    return true;
}
}
