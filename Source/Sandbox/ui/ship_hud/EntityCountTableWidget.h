#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>
#include <Framework/Text/TextLayout.h>

#include "EntityCountTableWidget.generated.h"

class UGridPanel;
class UTextBlock;

UCLASS()
class SANDBOX_API UEntityCountTableWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(FTestEntityRegistry::EntityCounts const& new_counts);
    void set_team_colours(UTestTeamVisualData::FColourArray const& new_colours);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UGridPanel* entity_count_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> data_alignment{ETextJustify::Center};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TEnumAsByte<ETextJustify::Type> entity_type_alignment{ETextJustify::Left};
  private:
    void rebuild_table();
    void set_text_style(UTextBlock& text, ETextJustify::Type alignment) const;

    FTestEntityRegistry::EntityCounts entity_counts{};
    UTestTeamVisualData::FColourArray team_colours{};
};
