#pragma once

#include <ioj/presentation/top_killer_entry.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>
#include <Framework/Text/TextLayout.h>

#include <span>

#include "TopKillersWidget.generated.h"

class UGridPanel;
class UTextBlock;

namespace ioj {
UCLASS()
class SPACEGAMEPRESENTATION_API UTopKillersWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    static constexpr int32 max_entries{5};

    void set_top_killers(std::span<presentation::TopKillerEntry const> new_entries);
    void set_team_colours(UTestTeamVisualData::FColourArray const& new_colours);
    void apply_hud_style(FGameHudStyle const& style);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UGridPanel* top_killers_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> data_alignment{ETextJustify::Center};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> entity_alignment{ETextJustify::Left};
  private:
    void rebuild_table();
    void set_text_style(UTextBlock& text, ETextJustify::Type alignment) const;

    std::vector<presentation::TopKillerEntry> top_killers;
    UTestTeamVisualData::FColourArray team_colours{};
    TOptional<FGameHudStyle> hud_style_{};
};

} // namespace ioj
