#pragma once

#include <Sandbox/batch_game/TestTeamVisualData.h>
#include <Sandbox/ui/ship_hud/ShipHudKillData.h>
#include <Sandbox/utilities/enums.h>

#include <Blueprint/UserWidget.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <Framework/Text/TextLayout.h>

#include "TeamKillMatrixWidget.generated.h"

class UGridPanel;
class UTextBlock;

UCLASS()
class SANDBOX_API UTeamKillMatrixWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_team_kill_matrix(TConstArrayView<ml::ship_hud::FTeamKillMatrixRow> const new_rows);
    void set_team_colours(UTestTeamVisualData::FColourArray const& new_colours);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UGridPanel* team_kill_matrix_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> data_alignment{ETextJustify::Center};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> team_alignment{ETextJustify::Left};
  private:
    void rebuild_table();
    void set_text_style(UTextBlock& text, ETextJustify::Type alignment) const;

    TArray<ml::ship_hud::FTeamKillMatrixRow> team_kill_matrix{};
    UTestTeamVisualData::FColourArray team_colours{};
};
