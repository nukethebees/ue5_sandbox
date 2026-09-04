#include "SlateDslSmoke/GenerateSlateDslSmokeAssetCommandlet.h"

#include "SbxUIExperiments/SlateDslSmoke/SlateDslSmokeWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace {
FString const package_name{TEXT("/SandboxUI/Examples/EUW_SlateDslSmoke")};
FName const asset_name{TEXT("EUW_SlateDslSmoke")};
}

UGenerateSlateDslSmokeAssetCommandlet::UGenerateSlateDslSmokeAssetCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UGenerateSlateDslSmokeAssetCommandlet::Main(FString const&) {
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString())};
    if (auto const* existing{LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *object_path)};
        existing != nullptr) {
        auto const valid_parent{
            existing->GeneratedClass != nullptr &&
            existing->GeneratedClass->IsChildOf(USlateDslSmokeWidget::StaticClass())};
        if (!valid_parent) {
            UE_LOG(LogTemp, Error, TEXT("Existing Slate DSL smoke asset has the wrong parent."));
            return 1;
        }

        UE_LOG(LogTemp, Display, TEXT("Slate DSL smoke asset already exists."));
        return 0;
    }

    auto* const package{CreatePackage(*package_name)};
    auto* const factory{NewObject<UEditorUtilityWidgetBlueprintFactory>()};
    factory->ParentClass = USlateDslSmokeWidget::StaticClass();
    auto* const widget_blueprint{Cast<UEditorUtilityWidgetBlueprint>(
        factory->FactoryCreateNew(UEditorUtilityWidgetBlueprint::StaticClass(),
                                  package,
                                  asset_name,
                                  RF_Public | RF_Standalone | RF_Transactional,
                                  nullptr,
                                  GWarn))};
    if (widget_blueprint == nullptr || widget_blueprint->Status == BS_Error) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create the Slate DSL smoke asset."));
        return 1;
    }

    FKismetEditorUtilities::CompileBlueprint(widget_blueprint);
    FAssetRegistryModule::AssetCreated(widget_blueprint);
    package->MarkPackageDirty();

    auto const filename{FPackageName::LongPackageNameToFilename(
        package_name, FPackageName::GetAssetPackageExtension())};
    FSavePackageArgs save_args;
    save_args.TopLevelFlags = RF_Public | RF_Standalone;
    save_args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(package, widget_blueprint, *filename, save_args)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to save '%s'."), *filename);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("Created '%s'."), *filename);
    return 0;
}
