#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>
#include <Framework/Text/TextLayout.h>

#include "TopKillersWidget.generated.h"

class UGridPanel;
class UTextBlock;

struct FTopKillerEntry {
    TestEntityUniqueId entity_id{};
    ETestEntityType entity_type{ETestEntityType::PlayerShip};
    ETestTeam team{ETestTeam::White};
    int32 kills{0};
};

UCLASS()
class SANDBOX_API UTopKillersWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    static constexpr int32 max_entries{5};

    void set_top_killers(TConstArrayView<FTopKillerEntry> const new_entries);
    void set_team_colours(UTestTeamVisualData::FColourArray const& new_colours);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;

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

    TArray<FTopKillerEntry> top_killers{};
    UTestTeamVisualData::FColourArray team_colours{};
};
