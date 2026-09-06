#pragma once

#include "Blueprint/UserWidget.h"

#include "LevelSelectWidget.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    ULevelSelectWidget(FObjectInitializer const& object_initializer = FObjectInitializer::Get());

    void prepare_for_open(FName preferred_level_id) noexcept;
    virtual void refresh() {}
    virtual void focus_primary_action() {}
  protected:
    [[nodiscard]] auto get_preferred_level_id() const noexcept -> FName {
        return preferred_level_id_;
    }
  private:
    FName preferred_level_id_{NAME_None};
};
}
