#pragma once

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/presentation/widgets/ShipHudKillData.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>
#include <Framework/Text/TextLayout.h>

#include "TeamEntityTableWidget.generated.h"

class UGridPanel;
class UTextBlock;

UCLASS()
class SPACEGAME_API UTeamEntityTableWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(FTestEntityRegistry::EntityCounts const& new_counts);
    void set_team_kill_matrix(ml::ship_hud::FTeamKillMatrix const& new_matrix);
    void set_team_colours(UTestTeamVisualData::FColourArray const& new_colours);
    void set_font_size(int32 new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
    void set_show_team_totals(bool show_totals);
    auto get_show_team_totals() const noexcept -> bool { return show_team_totals; }
  protected:
    void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UGridPanel* team_entity_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    bool show_team_totals{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> data_alignment{ETextJustify::Center};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> entity_type_alignment{ETextJustify::Left};
  private:
    void rebuild_table();
    void set_text_style(UTextBlock& text, ETextJustify::Type alignment) const;

    FTestEntityRegistry::EntityCounts values{};
    UTestTeamVisualData::FColourArray team_colours{};
};
