#include "SandboxEditor/SandboxEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"

#include "Sandbox/combat/projectiles/data_assets/BulletDataAsset.h"
// #include "Sandbox/players/playable/data/player_skills.h"
#include "SandboxEditor/codegen/DataAssetCodeGenerator.h"
#include "SandboxEditor/codegen/TypedefCodeGenerator.h"
#include "SandboxEditor/slate/PlayerSkillsPropDisplay.h"
#include "SandboxEditor/slate/StrongTypedefPreview.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void FSandboxEditorModule::StartupModule() {
    constexpr auto logger{NestedLogger<"StartupModule">()};
    logger.log_verbose(TEXT("Module starting up!"));
    // No factory registration needed - MaterialExpressions control their own UI

    // Register menu extensions after ToolMenus module is loaded
    if (UToolMenus::IsToolMenuUIEnabled()) {
        register_menu_extensions();
    } else {
        // Delay registration until ToolMenus is ready
        FCoreDelegates::OnPostEngineInit.AddLambda([this]() {
            if (UToolMenus::IsToolMenuUIEnabled()) {
                register_menu_extensions();
            }
        });
    }

    auto& property_module{
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor")};
    property_module.RegisterCustomPropertyTypeLayout(
        "Dimensions",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(
            &FStrongTypedefPreview::MakeInstance));
    property_module.RegisterCustomPropertyTypeLayout(
        "PlayerSkills",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(
            &FPlayerSkillsPropDisplay::MakeInstance));
}

void FSandboxEditorModule::ShutdownModule() {
    // ToolMenus are automatically cleaned up when the module is unloaded
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor")) {
        auto& property_module{
            FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")};

        property_module.UnregisterCustomPropertyTypeLayout("Dimensions");
    }
}

void FSandboxEditorModule::register_menu_extensions() {
    constexpr auto logger{NestedLogger<"register_menu_extensions">()};

    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    TRY_INIT_PTR(menu, tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "GenerateDataAssetCode",
        FUIAction(
            FExecuteAction::CreateRaw(this, &FSandboxEditorModule::on_generate_data_asset_code)),
        FText::FromString("Generate Asset Code"),
        FText::FromString("Generate C++ accessor code for data assets"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile")));

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "GenerateTypedefs",
        FUIAction(FExecuteAction::CreateRaw(this, &FSandboxEditorModule::on_generate_typedefs)),
        FText::FromString("Generate Typedefs"),
        FText::FromString("Generate strong typedef wrapper structs"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile")));

    logger.log_log(TEXT("Registered menu extensions"));
}

void FSandboxEditorModule::on_generate_data_asset_code() {
    constexpr auto logger{NestedLogger<"on_generate_data_asset_code">()};
    logger.log_log(TEXT("Generating data asset code..."));

    // Configuration for bullet data assets
    FString const scan_path{TEXT("/Game/DataAssets")};
    UClass* asset_class{UBulletDataAsset::StaticClass()};
    FString const generated_class_name{TEXT("BulletAssetRegistry")};
    FString const output_directory{FPaths::ProjectDir() /
                                   TEXT("Source/Sandbox/generated/data_asset_registries/")};
    TArray<FString> const additional_includes{
        TEXT("Sandbox/combat/projectiles/data_assets/BulletDataAsset.h")};
    FString const namespace_name{TEXT("ml")};

    // Generate the code
    bool const success{FDataAssetCodeGenerator::generate_asset_registry(scan_path,
                                                                        asset_class,
                                                                        generated_class_name,
                                                                        output_directory,
                                                                        additional_includes,
                                                                        namespace_name)};

    if (success) {
        logger.log_log(TEXT("Code generation completed successfully!"));
    } else {
        logger.log_error(TEXT("Code generation failed!"));
    }
}

void FSandboxEditorModule::on_generate_typedefs() {
    constexpr auto logger{NestedLogger<"on_generate_typedefs">()};
    logger.log_log(TEXT("Generating strong typedefs..."));

    bool const success{FTypedefCodeGenerator::generate_typedefs()};

    if (success) {
        logger.log_log(TEXT("Typedef generation completed successfully!"));
    } else {
        logger.log_error(TEXT("Typedef generation failed!"));
    }
}

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
