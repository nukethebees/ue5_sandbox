#include "SandboxEditor/SandboxEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"

#include "Sandbox/data_assets/BulletDataAsset.h"
#include "SandboxEditor/DataAssetCodeGenerator.h"

void FSandboxEditorModule::StartupModule() {
    UE_LOG(LogTemp, Verbose, TEXT("SandboxEditor module starting up!"));
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
}

void FSandboxEditorModule::ShutdownModule() {
    // ToolMenus are automatically cleaned up when the module is unloaded
}

void FSandboxEditorModule::register_menu_extensions() {
    auto* tool_menus{UToolMenus::Get()};
    if (!tool_menus) {
        UE_LOG(LogTemp, Error, TEXT("SandboxEditor: Failed to get UToolMenus"));
        return;
    }

    // Extend the Level Editor toolbar
    auto* menu{tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User")};
    if (!menu) {
        UE_LOG(LogTemp, Error, TEXT("SandboxEditor: Failed to extend LevelEditor toolbar"));
        return;
    }

    // Add a new section for our custom tools
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    // Add the data asset code generator button
    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "GenerateDataAssetCode",
        FUIAction(
            FExecuteAction::CreateRaw(this, &FSandboxEditorModule::on_generate_data_asset_code)),
        FText::FromString("Generate Asset Code"),
        FText::FromString("Generate C++ accessor code for data assets"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile")));

    UE_LOG(LogTemp, Log, TEXT("SandboxEditor: Registered menu extensions"));
}

void FSandboxEditorModule::on_generate_data_asset_code() {
    UE_LOG(LogTemp, Log, TEXT("SandboxEditor: Generating data asset code..."));

    // Configuration for bullet data assets
    FString const scan_path{TEXT("/Game/DataAssets/Bullets")};
    UClass* asset_class{UBulletDataAsset::StaticClass()};
    FString const generated_class_name{TEXT("BulletAssetRegistry")};
    FString const output_directory{FPaths::ProjectDir() / TEXT("Source/Sandbox/generated/")};

    // Generate the code
    bool const success{FDataAssetCodeGenerator::generate_asset_registry(
        scan_path, asset_class, generated_class_name, output_directory)};

    if (success) {
        UE_LOG(LogTemp, Log, TEXT("SandboxEditor: Code generation completed successfully!"));
        // TODO: Show notification to user
    } else {
        UE_LOG(LogTemp, Error, TEXT("SandboxEditor: Code generation failed!"));
        // TODO: Show error notification to user
    }
}

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
