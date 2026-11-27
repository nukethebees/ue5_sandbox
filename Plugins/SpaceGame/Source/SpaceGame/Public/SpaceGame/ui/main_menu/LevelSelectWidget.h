#pragma once

#include <ioj/presentation/level_catalog.h>

#include "Blueprint/UserWidget.h"

#include "LevelSelectWidget.generated.h"

namespace ioj {
struct MenuUpdate;
UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    ULevelSelectWidget(FObjectInitializer const& object_initializer = FObjectInitializer::Get());

    virtual void refresh() {}
    virtual void present_catalog(presentation::LevelCatalogView const&) {}
    virtual void present(MenuUpdate const&) {}
};
}
