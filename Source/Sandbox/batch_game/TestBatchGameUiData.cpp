#include "Sandbox/batch_game/TestBatchGameUiData.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/ship_hud/MissionEntityHealthRowWidget.h"
#include "Sandbox/ui/ship_hud/MissionStatusWidget.h"
#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"
#include "Sandbox/ui/ship_hud/ShipHudWidget.h"
#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"
#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"
#include "Sandbox/ui/ship_hud/TeamEntityTableWidget.h"
#include "Sandbox/ui/ship_hud/TopKillersWidget.h"
#include "Sandbox/ui/widgets/DebugGraphWidget.h"
#include "SandboxGameShared/ui/widgets/ValueWidget.h"

#include <Misc/PackageName.h>
#include <UObject/UObjectGlobals.h>

auto ml::test_batch_game_ui_data::get_data_asset() -> UTestBatchGameUiData* {
    auto const data_asset_path{get_data_asset_path()};
    auto const data_asset_package_path{data_asset_path.ToString()};
    auto const data_asset_name{FPackageName::GetShortName(data_asset_package_path)};
    auto const data_asset_object_path{
        FString::Printf(TEXT("%s.%s"), *data_asset_package_path, *data_asset_name)};
    auto* const data_asset{LoadObject<UTestBatchGameUiData>(nullptr, *data_asset_object_path)};
    if (!IsValid(data_asset)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("Failed to load project UI data asset '%s'."),
               *data_asset_object_path);
    }

    return data_asset;
}

auto UTestBatchGameUiData::get_native_widget_classes() -> TConstArrayView<UClass*> {
    static UClass* const classes[]{
        UTeamEntityTableWidget::StaticClass(),
        UMissionEntityHealthRowWidget::StaticClass(),
        UMissionStatusWidget::StaticClass(),
        UShipHealthWidget::StaticClass(),
        UShipHudWidget::StaticClass(),
        UShipSpeedWidget::StaticClass(),
        UShipThrusterEnergyWidget::StaticClass(),
        UTopKillersWidget::StaticClass(),
        UDebugGraphWidget::StaticClass(),
        UValueWidget::StaticClass(),
    };
    return classes;
}

auto UTestBatchGameUiData::get_widget_class(UClass* const native_widget_class) const
    -> TSubclassOf<UUserWidget> {
    if (!IsValid(native_widget_class) ||
        !native_widget_class->IsChildOf(UUserWidget::StaticClass())) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UTestBatchGameUiData: Requested widget class is invalid: %s."),
               *GetNameSafe(native_widget_class));
        return nullptr;
    }

    auto const native_class{TSubclassOf<UUserWidget>{native_widget_class}};
    auto const* const mapped_class{widget_classes.classes.Find(native_class)};
    if (!mapped_class) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UTestBatchGameUiData: No WBP mapping exists for %s."),
               *native_widget_class->GetName());
        return nullptr;
    }

    auto* const widget_class{mapped_class->Get()};
    if (!IsValid(widget_class)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UTestBatchGameUiData: WBP mapping for %s is not set."),
               *native_widget_class->GetName());
        return nullptr;
    }

    if (!widget_class->IsChildOf(native_widget_class)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UTestBatchGameUiData: WBP %s is not derived from %s."),
               *widget_class->GetName(),
               *native_widget_class->GetName());
        return nullptr;
    }

    return widget_class;
}

#if WITH_EDITOR
void UTestBatchGameUiData::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    synchronise_widget_classes();
}

void UTestBatchGameUiData::PostLoad() {
    Super::PostLoad();

    synchronise_widget_classes();
}

void UTestBatchGameUiData::synchronise_widget_classes() {
    bool b_modified{false};
    auto const native_widget_classes{get_native_widget_classes()};
    auto const n_widget_classes{native_widget_classes.Num()};
    for (int32 i{0}; i < n_widget_classes; ++i) {
        auto const native_widget_class{TSubclassOf<UUserWidget>{native_widget_classes[i]}};
        if (!widget_classes.classes.Contains(native_widget_class)) {
            if (!b_modified) {
                Modify();
                b_modified = true;
            }
            widget_classes.classes.Add(native_widget_class, nullptr);
        }
    }

    if (b_modified) {
        MarkPackageDirty();
    }

    validate_widget_classes();
}

void UTestBatchGameUiData::validate_widget_classes() const {
    for (auto const& pair : widget_classes.classes) {
        auto* const native_widget_class{pair.Key.Get()};
        auto* const widget_class{pair.Value.Get()};
        if (!IsValid(native_widget_class)) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UTestBatchGameUiData: Widget mapping has an invalid native widget key."));
            continue;
        }

        if (IsValid(widget_class) && !widget_class->IsChildOf(native_widget_class)) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UTestBatchGameUiData: WBP %s is not derived from %s."),
                   *widget_class->GetName(),
                   *native_widget_class->GetName());
        }
    }
}
#endif
