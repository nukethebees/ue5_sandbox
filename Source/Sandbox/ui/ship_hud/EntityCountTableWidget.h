#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "EntityCountTableWidget.generated.h"

class UGridPanel;
class UTextBlock;

UCLASS()
class SANDBOX_API UEntityCountTableWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity_counts(ATestEntityRegistry::EntityCounts const& new_counts,
                           UTestTeamVisualData::FColourArray const& new_colours);
  protected:
    void NativePreConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UGridPanel* entity_count_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};
  private:
    void rebuild_table();
    void set_text_style(UTextBlock& text) const;

    ATestEntityRegistry::EntityCounts entity_counts{};
    UTestTeamVisualData::FColourArray team_colours{};
};
