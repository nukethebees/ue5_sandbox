#pragma once

#include "SpaceGame/entities/TestEntityRegistry.h"
#include "SpaceGame/entities/TestTeamVisualData.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>

#include "ForceStatusWidget.generated.h"

class SBox;

UCLASS()
class SPACEGAME_API UForceStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(FTestEntityRegistry::EntityCounts const& counts);
    void set_team_colours(UTestTeamVisualData::FColourArray const& colours);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
  protected:
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    auto build_content() const -> TSharedRef<SWidget>;
    void refresh_content();

    FTestEntityRegistry::EntityCounts entity_counts_{};
    UTestTeamVisualData::FColourArray team_colours_{};
    TOptional<ml::ioj::FGameHudStyle> hud_style_{};
    TSharedPtr<SBox> root_box_;
};
