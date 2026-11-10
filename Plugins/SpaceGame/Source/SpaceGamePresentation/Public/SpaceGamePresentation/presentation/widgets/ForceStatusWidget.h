#pragma once

#include "ioj/sim/entities/team_entity_array.h"
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>

#include "ForceStatusWidget.generated.h"

class SBox;

namespace ioj {
UCLASS()
class SPACEGAMEPRESENTATION_API UForceStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(sim::EntityCounts const& counts);
    void set_team_colours(UTestTeamVisualData::FColourArray const& colours);
    void apply_hud_style(FGameHudStyle const& style);
  protected:
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    auto build_content() const -> TSharedRef<SWidget>;
    void refresh_content();

    sim::EntityCounts entity_counts_{};
    UTestTeamVisualData::FColourArray team_colours_{};
    TOptional<FGameHudStyle> hud_style_{};
    TSharedPtr<SBox> root_box_;
};

} // namespace ioj
