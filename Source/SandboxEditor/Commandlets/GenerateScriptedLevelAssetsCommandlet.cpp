#include "SandboxEditor/Commandlets/GenerateScriptedLevelAssetsCommandlet.h"

#include <SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <AssetRegistry/AssetRegistryModule.h>
#include <Blueprint/WidgetTree.h>
#include <BlueprintEditorLibrary.h>
#include <Components/Button.h>
#include <Components/ButtonSlot.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/Overlay.h>
#include <Components/OverlaySlot.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>
#include <Engine/Blueprint.h>
#include <FileHelpers.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <Misc/PackageName.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <WidgetBlueprint.h>

namespace {
constexpr TCHAR widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect.WBP_LevelSelect")};
constexpr TCHAR main_menu_widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu.WBP_MainMenu")};
constexpr TCHAR runtime_config_package_name[]{TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig")};
constexpr TCHAR runtime_config_asset_name[]{TEXT("DA_GameRuntimeLevelConfig")};
constexpr TCHAR player_controller_object_path[]{
    TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController.BP_SpaceGamePlayerController")};
constexpr TCHAR player_controller_package_name[]{
    TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController")};
constexpr TCHAR player_controller_asset_name[]{TEXT("BP_SpaceGamePlayerController")};
constexpr TCHAR source_player_controller_object_path[]{
    TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/BP_TestSpaceShipController."
         "BP_TestSpaceShipController")};
constexpr TCHAR source_config_object_path[]{
    TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/DA_FT_soa_entities_LevelConfig."
         "DA_FT_soa_entities_LevelConfig")};
constexpr TCHAR runtime_map_package_name[]{TEXT("/SpaceGame/Levels/GameRuntime")};
constexpr TCHAR runtime_game_mode_class_path[]{
    TEXT("/Game/GameModes/BP_SpaceShipGameMode.BP_SpaceShipGameMode_C")};
constexpr TCHAR runtime_game_mode_object_path[]{
    TEXT("/Game/GameModes/BP_SpaceShipGameMode.BP_SpaceShipGameMode")};

template <typename T>
auto make_widget(UWidgetTree& tree, FName const name) -> T* {
    auto* const widget{tree.ConstructWidget<T>(T::StaticClass(), name)};
    widget->bIsVariable = true;
    return widget;
}

auto make_labelled_button(UWidgetTree& tree,
                          UHorizontalBox& parent,
                          FName const name,
                          FString const& label) -> UButton* {
    auto* const button{make_widget<UButton>(tree, name)};
    auto* const text{tree.ConstructWidget<UTextBlock>()};
    text->SetText(FText::FromString(label));
    auto* const content_slot{CastChecked<UButtonSlot>(button->AddChild(text))};
    content_slot->SetPadding(FMargin{12.0f, 6.0f});
    auto* const action_slot{parent.AddChildToHorizontalBox(button)};
    action_slot->SetPadding(FMargin{4.0f});
    return button;
}

auto save_asset(UObject& asset) -> bool {
    auto* const package{asset.GetOutermost()};
    package->MarkPackageDirty();
    auto const filename{FPackageName::LongPackageNameToFilename(
        package->GetName(), FPackageName::GetAssetPackageExtension())};
    FSavePackageArgs args;
    args.TopLevelFlags = RF_Public | RF_Standalone;
    args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(package, &asset, *filename, args);
}

auto generate_level_select_widget() -> bool {
    auto* const blueprint{LoadObject<UWidgetBlueprint>(nullptr, widget_object_path)};
    if (!IsValid(blueprint) || !IsValid(blueprint->WidgetTree)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load %s"), widget_object_path);
        return false;
    }

    blueprint->Modify();
    blueprint->WidgetTree->Rename(
        nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    blueprint->WidgetTree = NewObject<UWidgetTree>(blueprint, TEXT("WidgetTree"), RF_Transactional);
    auto& tree{*blueprint->WidgetTree};
    tree.Modify();

    auto* const root{make_widget<UOverlay>(tree, TEXT("root_widget"))};
    auto* const page{tree.ConstructWidget<UVerticalBox>()};
    auto* const page_slot{root->AddChildToOverlay(page)};
    page_slot->SetPadding(FMargin{32.0f});
    tree.RootWidget = root;

    auto* const heading{tree.ConstructWidget<UTextBlock>()};
    heading->SetText(FText::FromString(TEXT("Scripted Levels")));
    auto heading_font{heading->GetFont()};
    heading_font.Size = 28;
    heading->SetFont(heading_font);
    auto* const heading_slot{page->AddChildToVerticalBox(heading)};
    heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    auto* const scroll{tree.ConstructWidget<UScrollBox>()};
    auto* const level_list{make_widget<UVerticalBox>(tree, TEXT("level_list"))};
    scroll->AddChild(level_list);
    auto* const scroll_slot{page->AddChildToVerticalBox(scroll)};
    scroll_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    scroll_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    auto* const selected_file{make_widget<UTextBlock>(tree, TEXT("selected_file_text"))};
    auto* const selected_file_slot{page->AddChildToVerticalBox(selected_file)};
    selected_file_slot->SetPadding(FMargin{0.0f, 4.0f});
    auto* const title{make_widget<UTextBlock>(tree, TEXT("title_text"))};
    auto title_font{title->GetFont()};
    title_font.Size = 22;
    title->SetFont(title_font);
    auto* const title_slot{page->AddChildToVerticalBox(title)};
    title_slot->SetPadding(FMargin{0.0f, 4.0f});
    auto* const description{make_widget<UTextBlock>(tree, TEXT("description_text"))};
    description->SetAutoWrapText(true);
    auto* const description_slot{page->AddChildToVerticalBox(description)};
    description_slot->SetPadding(FMargin{0.0f, 4.0f, 0.0f, 8.0f});
    auto* const status{make_widget<UTextBlock>(tree, TEXT("status_text"))};
    status->SetAutoWrapText(true);
    auto* const status_slot{page->AddChildToVerticalBox(status)};
    status_slot->SetPadding(FMargin{0.0f, 4.0f, 0.0f, 8.0f});

    auto* const actions{tree.ConstructWidget<UHorizontalBox>()};
    page->AddChildToVerticalBox(actions);
    make_labelled_button(tree, *actions, TEXT("refresh_button"), TEXT("Refresh"));
    make_labelled_button(tree, *actions, TEXT("launch_button"), TEXT("Launch"));
    make_labelled_button(tree, *actions, TEXT("start_paused_button"), TEXT("Start Paused"));
    make_labelled_button(tree, *actions, TEXT("back_button"), TEXT("Back"));

    blueprint->WidgetVariableNameToGuidMap.Reset();
    tree.ForEachWidget(
        [blueprint](UWidget* const widget) { blueprint->OnVariableAdded(widget->GetFName()); });

    auto* const widget_class{ml::s7::UScriptLevelSelectWidget::StaticClass()};
    if (blueprint->ParentClass != widget_class) {
        UBlueprintEditorLibrary::ReparentBlueprint(blueprint, widget_class);
    }
    FKismetEditorUtilities::CompileBlueprint(blueprint);
    if (blueprint->Status == BS_Error) {
        UE_LOG(LogTemp, Error, TEXT("WBP_LevelSelect failed to compile"));
        return false;
    }
    return save_asset(*blueprint);
}

auto recompile_main_menu_widget() -> bool {
    auto* const blueprint{LoadObject<UWidgetBlueprint>(nullptr, main_menu_widget_object_path)};
    if (!IsValid(blueprint)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load %s"), main_menu_widget_object_path);
        return false;
    }

    FKismetEditorUtilities::CompileBlueprint(blueprint);
    if (blueprint->Status == BS_Error) {
        UE_LOG(LogTemp, Error, TEXT("WBP_MainMenu failed to compile"));
        return false;
    }
    return save_asset(*blueprint);
}

auto load_or_create_player_controller() -> UBlueprint* {
    auto* blueprint{LoadObject<UBlueprint>(nullptr, player_controller_object_path)};
    if (!IsValid(blueprint)) {
        auto* const source{LoadObject<UBlueprint>(nullptr, source_player_controller_object_path)};
        if (!IsValid(source)) {
            UE_LOG(LogTemp, Error, TEXT("Could not load source player controller Blueprint"));
            return nullptr;
        }

        auto* const package{CreatePackage(player_controller_package_name)};
        blueprint = Cast<UBlueprint>(StaticDuplicateObject(
            source, package, player_controller_asset_name, RF_Public | RF_Standalone));
        if (!IsValid(blueprint)) {
            UE_LOG(LogTemp, Error, TEXT("Could not duplicate player controller Blueprint"));
            return nullptr;
        }
        FAssetRegistryModule::AssetCreated(blueprint);
    }

    if (blueprint->ParentClass != ASpaceGamePlayerController::StaticClass()) {
        UBlueprintEditorLibrary::ReparentBlueprint(blueprint,
                                                   ASpaceGamePlayerController::StaticClass());
    }
    FKismetEditorUtilities::CompileBlueprint(blueprint);
    if (blueprint->Status == BS_Error || !IsValid(blueprint->GeneratedClass)) {
        UE_LOG(LogTemp, Error, TEXT("BP_SpaceGamePlayerController failed to compile"));
        return nullptr;
    }
    return save_asset(*blueprint) ? blueprint : nullptr;
}

auto configure_runtime_game_mode(UClass& player_controller_class) -> bool {
    auto* const blueprint{LoadObject<UBlueprint>(nullptr, runtime_game_mode_object_path)};
    if (!IsValid(blueprint) || !IsValid(blueprint->GeneratedClass)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load runtime game mode Blueprint"));
        return false;
    }

    auto* const game_mode{Cast<AGameModeBase>(blueprint->GeneratedClass->GetDefaultObject())};
    if (!IsValid(game_mode)) {
        UE_LOG(LogTemp, Error, TEXT("Runtime game mode has an invalid default object"));
        return false;
    }
    game_mode->Modify();
    game_mode->PlayerControllerClass = &player_controller_class;
    return save_asset(*blueprint);
}

auto load_or_create_runtime_config(UClass& player_controller_class) -> USpaceGameLevelConfig* {
    auto const object_path{
        FString::Printf(TEXT("%s.%s"), runtime_config_package_name, runtime_config_asset_name)};
    auto* config{LoadObject<USpaceGameLevelConfig>(nullptr, *object_path)};
    if (!IsValid(config)) {
        auto* const source{LoadObject<USpaceGameLevelConfig>(nullptr, source_config_object_path)};
        if (!IsValid(source)) {
            UE_LOG(LogTemp, Error, TEXT("Could not load source level config"));
            return nullptr;
        }

        auto* const package{CreatePackage(runtime_config_package_name)};
        config = Cast<USpaceGameLevelConfig>(StaticDuplicateObject(
            source, package, runtime_config_asset_name, RF_Public | RF_Standalone));
        if (!IsValid(config)) {
            UE_LOG(LogTemp, Error, TEXT("Could not duplicate runtime level config"));
            return nullptr;
        }
        FAssetRegistryModule::AssetCreated(config);
    }

    config->Modify();
    config->classes.player_controller_class = &player_controller_class;
    return save_asset(*config) ? config : nullptr;
}

auto generate_runtime_map() -> bool {
    auto* const controller_blueprint{load_or_create_player_controller()};
    auto* const controller_class{
        IsValid(controller_blueprint) ? controller_blueprint->GeneratedClass.Get() : nullptr};
    if (!IsValid(controller_class) || !configure_runtime_game_mode(*controller_class)) {
        return false;
    }

    auto* const config{load_or_create_runtime_config(*controller_class)};
    if (!IsValid(config)) {
        return false;
    }

    auto* const world{UEditorLoadingAndSavingUtils::NewBlankMap(false)};
    if (!IsValid(world)) {
        UE_LOG(LogTemp, Error, TEXT("Could not create blank runtime map"));
        return false;
    }

    auto* const game_mode_class{LoadClass<AGameModeBase>(nullptr, runtime_game_mode_class_path)};
    auto* const world_settings{world->GetWorldSettings()};
    if (!IsValid(game_mode_class) || !IsValid(world_settings)) {
        UE_LOG(LogTemp, Error, TEXT("Could not configure the runtime game mode"));
        return false;
    }
    world_settings->DefaultGameMode = game_mode_class;

    auto* const orchestrator{world->SpawnActor<ATestBatchOrchestrator>()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogTemp, Error, TEXT("Could not spawn runtime orchestrator"));
        return false;
    }
    orchestrator->set_level_config(*config);
    orchestrator->set_start_mode(EOrchestratorStartMode::AuthoredLevel);

    auto* const starfield{world->SpawnActor<AGpuStarfieldExperimentActor>()};
    if (!IsValid(starfield)) {
        UE_LOG(LogTemp, Error, TEXT("Could not spawn runtime GPU starfield"));
        return false;
    }

    auto const filename{FPackageName::LongPackageNameToFilename(
        runtime_map_package_name, FPackageName::GetMapPackageExtension())};
    return FEditorFileUtils::SaveLevel(world->PersistentLevel, filename);
}
}

UGenerateScriptedLevelAssetsCommandlet::UGenerateScriptedLevelAssetsCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UGenerateScriptedLevelAssetsCommandlet::Main(FString const&) {
    auto const widget_generated{generate_level_select_widget()};
    auto const main_menu_generated{widget_generated && recompile_main_menu_widget()};
    auto const map_generated{generate_runtime_map()};
    return main_menu_generated && map_generated ? 0 : 1;
}
