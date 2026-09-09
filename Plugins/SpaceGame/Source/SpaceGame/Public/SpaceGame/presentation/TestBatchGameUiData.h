#pragma once
#include <SpaceGamePresentation/presentation/HudUpdateSettings.h>

#include <Blueprint/UserWidget.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "SpaceGamePresentation/presentation/HudCrosshairDistances.h"

#include "TestBatchGameUiData.generated.h"

class UTestBatchGameUiData;
class UTestTeamVisualData;

USTRUCT(BlueprintType)
struct FBatchGameUiClasses {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Widgets")
    TMap<TSubclassOf<UUserWidget>, TSubclassOf<UUserWidget>> classes{};
};

namespace ml::test_batch_game_ui_data {
inline auto get_data_asset_path() -> FName {
    return FName{TEXT("/Game/UI/DA_ui_data")};
}

auto SPACEGAME_API get_data_asset() -> UTestBatchGameUiData*;
}

UCLASS(BlueprintType)
class SPACEGAME_API UTestBatchGameUiData : public UDataAsset {
    GENERATED_BODY()
  public:
    static auto get_native_widget_classes() -> TConstArrayView<UClass*>;

    auto get_widget_class(UClass* native_widget_class) const -> TSubclassOf<UUserWidget>;

    template <typename WidgetType>
    auto get_widget_class() const -> TSubclassOf<WidgetType> {
        static_assert(TIsDerivedFrom<WidgetType, UUserWidget>::IsDerived,
                      "WidgetType must derive from UUserWidget.");

        auto const widget_class{get_widget_class(WidgetType::StaticClass())};
        return TSubclassOf<WidgetType>{widget_class.Get()};
    }

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& event) override;
    void PostLoad() override;
#endif

    UPROPERTY(EditAnywhere, Category = "Widget Classes")
    FBatchGameUiClasses widget_classes{};

    UPROPERTY(EditAnywhere, Category = "Colours")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    FHudCrosshairDistances crosshair_distances{};
  private:
#if WITH_EDITOR
    void synchronise_widget_classes();
    void validate_widget_classes() const;
#endif
};
