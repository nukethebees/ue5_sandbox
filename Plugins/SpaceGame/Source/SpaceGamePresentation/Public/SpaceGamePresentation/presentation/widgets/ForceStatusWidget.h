#pragma once

#include "ioj/sim/entity_registry.h"
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>

#include "ForceStatusWidget.generated.h"

class SBox;

UCLASS()
class SPACEGAMEPRESENTATION_API UForceStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(::ioj::sim::EntityRegistry::EntityCounts const& counts);
    void set_team_colours(UTestTeamVisualData::FColourArray const& colours);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
  protected:
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    auto build_content() const -> TSharedRef<SWidget>;
    void refresh_content();

    ::ioj::sim::EntityRegistry::EntityCounts entity_counts_{};
    UTestTeamVisualData::FColourArray team_colours_{};
    TOptional<ml::ioj::FGameHudStyle> hud_style_{};
    TSharedPtr<SBox> root_box_;
};
