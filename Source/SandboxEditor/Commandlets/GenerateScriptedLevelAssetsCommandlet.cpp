#include "SandboxEditor/Commandlets/GenerateScriptedLevelAssetsCommandlet.h"

#include <SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <AssetRegistry/AssetRegistryModule.h>
#include <Blueprint/WidgetTree.h>
#include <BlueprintEditorLibrary.h>
#include <CommonTextBlock.h>
#include <Components/Border.h>
#include <Components/Button.h>
#include <Components/ButtonSlot.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/NativeWidgetHost.h>
#include <Components/Overlay.h>
#include <Components/OverlaySlot.h>
#include <Components/ScrollBox.h>
#include <Components/ScrollBoxSlot.h>
#include <Components/SizeBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>
#include <Components/WidgetSwitcher.h>
#include <Engine/Blueprint.h>
#include <FileHelpers.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <InputAction.h>
#include <InputCoreTypes.h>
#include <InputMappingContext.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <Misc/PackageName.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <WidgetBlueprint.h>
#include <WidgetBlueprintOperationUtils.h>
#include <Widgets/CommonActivatableWidgetContainer.h>

namespace {
constexpr TCHAR widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect.WBP_LevelSelect")};
constexpr TCHAR main_menu_widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu.WBP_MainMenu")};
constexpr TCHAR main_menu_widget_package_name[]{TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu")};
constexpr TCHAR pause_menu_widget_object_path[]{
    TEXT("/Game/UI/pause_menu/WBP_PauseMenu.WBP_PauseMenu")};
constexpr TCHAR pause_menu_widget_package_name[]{TEXT("/Game/UI/pause_menu/WBP_PauseMenu")};
constexpr TCHAR completion_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_LevelCompletion.WBP_LevelCompletion")};
constexpr TCHAR completion_widget_package_name[]{TEXT("/SpaceGame/UI/InGame/WBP_LevelCompletion")};
constexpr TCHAR menu_button_package_name[]{TEXT("/SpaceGame/UI/Common/WBP_MenuButton")};
constexpr TCHAR menu_button_object_path[]{
    TEXT("/SpaceGame/UI/Common/WBP_MenuButton.WBP_MenuButton")};
constexpr TCHAR root_layout_package_name[]{TEXT("/SpaceGame/UI/Common/WBP_GameUiRoot")};
constexpr TCHAR root_layout_object_path[]{
    TEXT("/SpaceGame/UI/Common/WBP_GameUiRoot.WBP_GameUiRoot")};
constexpr TCHAR options_widget_class_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_Options.WBP_Options_C")};
constexpr TCHAR save_game_viewer_class_path[]{
    TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameViewer.WBP_SaveGameViewer_C")};
constexpr TCHAR back_action_package_name[]{TEXT("/SpaceGame/Input/UI/IA_menu_back")};
constexpr TCHAR back_action_object_path[]{TEXT("/SpaceGame/Input/UI/IA_menu_back.IA_menu_back")};
constexpr TCHAR menu_mapping_package_name[]{TEXT("/SpaceGame/Input/UI/IMC_menu")};
constexpr TCHAR menu_mapping_object_path[]{TEXT("/SpaceGame/Input/UI/IMC_menu.IMC_menu")};
constexpr TCHAR global_mapping_object_path[]{
    TEXT("/SpaceGame/Input/Player/IMC_Player_Global.IMC_Player_Global")};
constexpr TCHAR pause_action_object_path[]{TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause")};
FName const generation_context{TEXT("GenerateScriptedLevelAssets")};
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
                          UVerticalBox& parent,
                          FName const name,
                          FString const& label) -> UButton* {
    auto* const button{make_widget<UButton>(tree, name)};
    auto* const text{tree.ConstructWidget<UTextBlock>()};
    text->SetText(FText::FromString(label));
    auto* const content_slot{CastChecked<UButtonSlot>(button->AddChild(text))};
    content_slot->SetPadding(FMargin{12.0f, 6.0f});
    auto* const action_slot{parent.AddChildToVerticalBox(button)};
    action_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
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

auto load_or_create_widget_blueprint(TCHAR const* const object_path,
                                     TCHAR const* const package_name,
                                     FName const asset_name,
                                     UClass& parent_class) -> UWidgetBlueprint* {
    auto* blueprint{LoadObject<UWidgetBlueprint>(nullptr, object_path)};
    if (!IsValid(blueprint)) {
        auto* const package{CreatePackage(package_name)};
        blueprint =
            FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(package,
                                                                  asset_name,
                                                                  BPTYPE_Normal,
                                                                  UUserWidget::StaticClass(),
                                                                  nullptr,
                                                                  generation_context,
                                                                  false);
        if (!IsValid(blueprint)) {
            UE_LOG(LogTemp, Error, TEXT("Could not create %s"), object_path);
            return nullptr;
        }
        FAssetRegistryModule::AssetCreated(blueprint);
    }

    if (blueprint->ParentClass != &parent_class) {
        UBlueprintEditorLibrary::ReparentBlueprint(blueprint, &parent_class);
    }
    blueprint->Modify();
    if (IsValid(blueprint->WidgetTree)) {
        blueprint->WidgetTree->Rename(
            nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
    blueprint->WidgetTree = NewObject<UWidgetTree>(blueprint, TEXT("WidgetTree"), RF_Transactional);
    blueprint->WidgetVariableNameToGuidMap.Reset();
    return blueprint;
}

auto compile_and_save(UWidgetBlueprint& blueprint) -> bool {
    blueprint.WidgetTree->ForEachWidget(
        [&blueprint](UWidget* const widget) { blueprint.OnVariableAdded(widget->GetFName()); });
    FKismetEditorUtilities::CompileBlueprint(&blueprint);
    if (blueprint.Status == BS_Error) {
        UE_LOG(LogTemp, Error, TEXT("%s failed to compile"), *blueprint.GetName());
        return false;
    }
    return save_asset(blueprint);
}

auto make_menu_button(UWidgetTree& tree,
                      UVerticalBox& parent,
                      UClass& button_class,
                      FName const name,
                      TCHAR const* const label) -> ml::ioj::UMenuButtonWidget* {
    auto* const button{tree.ConstructWidget<ml::ioj::UMenuButtonWidget>(&button_class, name)};
    button->bIsVariable = true;
    button->set_text(FText::FromString(label));
    auto* const slot{parent.AddChildToVerticalBox(button)};
    slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    return button;
}

auto add_pause_stat_row(UWidgetTree& tree,
                        UGridPanel& grid,
                        int32 const row,
                        FName const label_name,
                        FText const& label,
                        FName const value_name) -> UTextBlock* {
    auto* const label_widget{make_widget<UTextBlock>(tree, label_name)};
    label_widget->SetText(label);
    label_widget->SetAutoWrapText(true);
    auto* const label_slot{grid.AddChildToGrid(label_widget, row, 0)};
    label_slot->SetPadding(FMargin{0.0f, 4.0f, 16.0f, 4.0f});
    label_slot->SetVerticalAlignment(VAlign_Center);

    auto* const value_widget{make_widget<UTextBlock>(tree, value_name)};
    value_widget->SetText(FText::AsNumber(0));
    value_widget->SetJustification(ETextJustify::Right);
    auto* const value_slot{grid.AddChildToGrid(value_widget, row, 1)};
    value_slot->SetPadding(FMargin{0.0f, 4.0f});
    value_slot->SetHorizontalAlignment(HAlign_Fill);
    value_slot->SetVerticalAlignment(VAlign_Center);
    return value_widget;
}

auto generate_menu_button_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(menu_button_object_path,
                                        menu_button_package_name,
                                        TEXT("WBP_MenuButton"),
                                        *ml::ioj::UMenuButtonWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    auto* const background{make_widget<UBorder>(*blueprint->WidgetTree, TEXT("background"))};
    background->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* const label{make_widget<UCommonTextBlock>(*blueprint->WidgetTree, TEXT("label_text"))};
    label->SetJustification(ETextJustify::Center);
    label->SetVisibility(ESlateVisibility::HitTestInvisible);
    background->SetContent(label);
    blueprint->WidgetTree->RootWidget = background;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_root_layout_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(root_layout_object_path,
                                        root_layout_package_name,
                                        TEXT("WBP_GameUiRoot"),
                                        *ml::ioj::UGameUiRootLayout::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    auto& tree{*blueprint->WidgetTree};
    auto* const root{tree.ConstructWidget<UOverlay>()};
    auto* const screen_stack{
        make_widget<UCommonActivatableWidgetStack>(tree, TEXT("screen_stack"))};
    auto* const modal_stack{make_widget<UCommonActivatableWidgetStack>(tree, TEXT("modal_stack"))};
    for (auto* const stack : {screen_stack, modal_stack}) {
        stack->SetTransitionDuration(0.0f);
        auto* const slot{root->AddChildToOverlay(stack)};
        slot->SetHorizontalAlignment(HAlign_Fill);
        slot->SetVerticalAlignment(VAlign_Fill);
    }
    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_pause_menu_widget(UClass& button_class) -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(pause_menu_widget_object_path,
                                        pause_menu_widget_package_name,
                                        TEXT("WBP_PauseMenu"),
                                        *ml::ioj::UPauseMenuWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("root_widget"))};
    auto* const panel{tree.ConstructWidget<UHorizontalBox>()};
    auto* const panel_slot{root->AddChildToOverlay(panel)};
    panel_slot->SetPadding(FMargin{80.0f});
    panel_slot->SetHorizontalAlignment(HAlign_Fill);
    panel_slot->SetVerticalAlignment(VAlign_Fill);

    auto* const actions{tree.ConstructWidget<UVerticalBox>()};
    auto* const actions_slot{panel->AddChildToHorizontalBox(actions)};
    actions_slot->SetSize(FSlateChildSize{ESlateSizeRule::Automatic});
    actions_slot->SetPadding(FMargin{0.0f, 0.0f, 40.0f, 0.0f});
    auto* const paused{make_widget<UTextBlock>(tree, TEXT("paused_heading"))};
    paused->SetText(FText::FromString(TEXT("Paused")));
    actions->AddChildToVerticalBox(paused)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 18.0f});
    make_menu_button(tree, *actions, button_class, TEXT("resume_button"), TEXT("Resume"));
    make_menu_button(tree, *actions, button_class, TEXT("overview_button"), TEXT("Overview"));
    make_menu_button(tree, *actions, button_class, TEXT("stats_button"), TEXT("Stats"));
    make_menu_button(tree, *actions, button_class, TEXT("options_button"), TEXT("Options"));
    make_menu_button(tree,
                     *actions,
                     button_class,
                     TEXT("return_to_level_select_button"),
                     TEXT("Return to Level Select"));
    make_menu_button(tree, *actions, button_class, TEXT("quit_button"), TEXT("Quit Game"));

    auto* const page{tree.ConstructWidget<UVerticalBox>()};
    panel->AddChildToHorizontalBox(page)->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    auto* const heading{make_widget<UTextBlock>(tree, TEXT("page_heading"))};
    page->AddChildToVerticalBox(heading)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 16.0f});

    auto* const switcher{make_widget<UWidgetSwitcher>(tree, TEXT("page_switcher"))};
    page->AddChildToVerticalBox(switcher)->SetSize(FSlateChildSize{ESlateSizeRule::Fill});

    auto* const overview{make_widget<UTextBlock>(tree, TEXT("overview_placeholder"))};
    overview->SetText(FText::FromString(TEXT("Overview placeholder content")));
    overview->SetAutoWrapText(true);
    switcher->AddChild(overview);

    auto* const stats_scroll{tree.ConstructWidget<UScrollBox>()};
    auto* const stats_contents{tree.ConstructWidget<UVerticalBox>()};
    auto* const stats_contents_slot{
        CastChecked<UScrollBoxSlot>(stats_scroll->AddChild(stats_contents))};
    stats_contents_slot->SetHorizontalAlignment(HAlign_Fill);

    auto* const summary_heading{make_widget<UTextBlock>(tree, TEXT("stats_summary_heading"))};
    summary_heading->SetText(FText::FromString(TEXT("Level Summary")));
    stats_contents->AddChildToVerticalBox(summary_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});

    auto* const summary_panel{make_widget<UBorder>(tree, TEXT("stats_summary_panel"))};
    auto* const summary_grid{tree.ConstructWidget<UGridPanel>()};
    summary_grid->SetColumnFill(0, 0.7f);
    summary_grid->SetColumnFill(1, 0.3f);
    add_pause_stat_row(tree,
                       *summary_grid,
                       0,
                       TEXT("stats_label_elapsed_time"),
                       FText::FromString(TEXT("Elapsed Time")),
                       TEXT("elapsed_time_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       1,
                       TEXT("stats_label_entities_spawned"),
                       FText::FromString(TEXT("Entities Spawned")),
                       TEXT("entities_spawned_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       2,
                       TEXT("stats_label_entities_active"),
                       FText::FromString(TEXT("Entities Active")),
                       TEXT("entities_active_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       3,
                       TEXT("stats_label_entities_destroyed"),
                       FText::FromString(TEXT("Entities Destroyed")),
                       TEXT("entities_destroyed_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       4,
                       TEXT("stats_label_kills"),
                       FText::FromString(TEXT("Kills")),
                       TEXT("kills_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       5,
                       TEXT("stats_label_lasers_fired"),
                       FText::FromString(TEXT("Lasers Fired")),
                       TEXT("lasers_fired_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       6,
                       TEXT("stats_label_lasers_active"),
                       FText::FromString(TEXT("Lasers Active")),
                       TEXT("lasers_active_value"));
    summary_panel->SetContent(summary_grid);
    stats_contents->AddChildToVerticalBox(summary_panel)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});

    auto* const graph_heading{make_widget<UTextBlock>(tree, TEXT("stats_graph_heading"))};
    graph_heading->SetText(FText::FromString(TEXT("Entity Activity")));
    stats_contents->AddChildToVerticalBox(graph_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 4.0f});
    auto* const graph_description{make_widget<UTextBlock>(tree, TEXT("stats_graph_description"))};
    graph_description->SetText(
        FText::FromString(TEXT("Active entities and kills over simulation time (seconds).")));
    graph_description->SetAutoWrapText(true);
    stats_contents->AddChildToVerticalBox(graph_description)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const graph_size{tree.ConstructWidget<USizeBox>()};
    graph_size->SetHeightOverride(260.0f);
    auto* const graph_host{make_widget<UNativeWidgetHost>(tree, TEXT("stats_graph_host"))};
    graph_size->SetContent(graph_host);
    stats_contents->AddChildToVerticalBox(graph_size)->SetHorizontalAlignment(HAlign_Fill);
    switcher->AddChild(stats_scroll);

    auto* const options{make_widget<UTextBlock>(tree, TEXT("options_placeholder"))};
    options->SetText(FText::FromString(TEXT("Options placeholder content")));
    options->SetAutoWrapText(true);
    switcher->AddChild(options);

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_level_completion_widget(UClass& button_class) -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(completion_widget_object_path,
                                        completion_widget_package_name,
                                        TEXT("WBP_LevelCompletion"),
                                        *ml::ioj::ULevelCompletionWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("root_widget"))};
    auto* const panel{tree.ConstructWidget<UVerticalBox>()};
    auto* const panel_slot{root->AddChildToOverlay(panel)};
    panel_slot->SetPadding(FMargin{80.0f});
    panel_slot->SetHorizontalAlignment(HAlign_Center);
    panel_slot->SetVerticalAlignment(VAlign_Center);

    auto* const heading{make_widget<UTextBlock>(tree, TEXT("mission_complete_text"))};
    heading->SetText(FText::FromString(TEXT("Mission Complete")));
    auto heading_font{heading->GetFont()};
    heading_font.Size = 38;
    heading->SetFont(heading_font);
    panel->AddChildToVerticalBox(heading)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    auto* const level_name{make_widget<UTextBlock>(tree, TEXT("level_name_text"))};
    auto level_name_font{level_name->GetFont()};
    level_name_font.Size = 28;
    level_name->SetFont(level_name_font);
    panel->AddChildToVerticalBox(level_name)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 24.0f});

    auto* const statistics{make_widget<UVerticalBox>(tree, TEXT("statistics_container"))};
    panel->AddChildToVerticalBox(statistics)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 24.0f});
    make_menu_button(tree,
                     *panel,
                     button_class,
                     TEXT("return_to_level_select_button"),
                     TEXT("Return to Level Select"));
    make_menu_button(tree, *panel, button_class, TEXT("keep_playing_button"), TEXT("Keep Playing"));

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_main_menu_widget(UClass& button_class) -> UClass* {
    auto* const options_class{
        LoadClass<ml::ioj::UOptionsWidget>(nullptr, options_widget_class_path)};
    auto* const save_viewer_class{
        LoadClass<ml::ioj::USaveGameViewerWidget>(nullptr, save_game_viewer_class_path)};
    if (!IsValid(options_class) || !IsValid(save_viewer_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load nested main-menu widget classes"));
        return nullptr;
    }
    auto* const blueprint{
        load_or_create_widget_blueprint(main_menu_widget_object_path,
                                        main_menu_widget_package_name,
                                        TEXT("WBP_MainMenu"),
                                        *ml::ioj::UMainMenuWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("root_widget"))};
    auto* const switcher{make_widget<UWidgetSwitcher>(tree, TEXT("page_switcher"))};
    auto* const switcher_slot{root->AddChildToOverlay(switcher)};
    switcher_slot->SetPadding(FMargin{80.0f});
    switcher_slot->SetHorizontalAlignment(HAlign_Fill);
    switcher_slot->SetVerticalAlignment(VAlign_Fill);

    auto* const main_page{make_widget<UVerticalBox>(tree, TEXT("main_page"))};
    auto* const heading{tree.ConstructWidget<UTextBlock>()};
    heading->SetText(FText::FromString(TEXT("Nuke the Bees")));
    auto heading_font{heading->GetFont()};
    heading_font.Size = 38;
    heading->SetFont(heading_font);
    main_page->AddChildToVerticalBox(heading)->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});
    make_menu_button(tree, *main_page, button_class, TEXT("play_button"), TEXT("Play"));
    make_menu_button(tree, *main_page, button_class, TEXT("save_games_button"), TEXT("Save Games"));
    make_menu_button(tree, *main_page, button_class, TEXT("options_button"), TEXT("Options"));
    make_menu_button(tree, *main_page, button_class, TEXT("quit_button"), TEXT("Quit"));
    switcher->AddChild(main_page);

    auto* const save_page{make_widget<UVerticalBox>(tree, TEXT("save_games_page"))};
    auto* const save_viewer{tree.ConstructWidget<ml::ioj::USaveGameViewerWidget>(
        save_viewer_class, TEXT("save_game_viewer"))};
    save_viewer->bIsVariable = true;
    save_page->AddChildToVerticalBox(save_viewer)->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    make_labelled_button(tree, *save_page, TEXT("save_games_back_button"), TEXT("Back"));
    switcher->AddChild(save_page);

    auto* const options{
        tree.ConstructWidget<ml::ioj::UOptionsWidget>(options_class, TEXT("options_widget"))};
    options->bIsVariable = true;
    switcher->AddChild(options);
    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

template <typename T>
auto load_or_create_asset(TCHAR const* const object_path,
                          TCHAR const* const package_name,
                          FName const asset_name) -> T* {
    auto* asset{LoadObject<T>(nullptr, object_path)};
    if (!IsValid(asset)) {
        auto* const package{CreatePackage(package_name)};
        asset = NewObject<T>(package, asset_name, RF_Public | RF_Standalone);
        if (IsValid(asset)) {
            FAssetRegistryModule::AssetCreated(asset);
        }
    }
    return asset;
}

auto generate_menu_input_assets() -> bool {
    auto* const back_action{load_or_create_asset<UInputAction>(
        back_action_object_path, back_action_package_name, TEXT("IA_menu_back"))};
    auto* const menu_mapping{load_or_create_asset<UInputMappingContext>(
        menu_mapping_object_path, menu_mapping_package_name, TEXT("IMC_menu"))};
    auto* const global_mapping{
        LoadObject<UInputMappingContext>(nullptr, global_mapping_object_path)};
    auto* const pause_action{LoadObject<UInputAction>(nullptr, pause_action_object_path)};
    if (!IsValid(back_action) || !IsValid(menu_mapping) || !IsValid(global_mapping) ||
        !IsValid(pause_action)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load or create menu input assets"));
        return false;
    }

    back_action->Modify();
    back_action->ValueType = EInputActionValueType::Boolean;
    menu_mapping->Modify();
    menu_mapping->UnmapAll();
    menu_mapping->MapKey(back_action, EKeys::Escape);
    menu_mapping->MapKey(back_action, EKeys::Gamepad_FaceButton_Right);

    bool pause_gamepad_mapping_exists{false};
    global_mapping->ForEachKeyMapping([&](FEnhancedActionKeyMapping const& mapping) {
        pause_gamepad_mapping_exists |=
            mapping.Action == pause_action && mapping.Key == EKeys::Gamepad_Special_Right;
    });
    if (!pause_gamepad_mapping_exists) {
        global_mapping->Modify();
        global_mapping->MapKey(pause_action, EKeys::Gamepad_Special_Right);
    }
    return save_asset(*back_action) && save_asset(*menu_mapping) && save_asset(*global_mapping);
}

auto configure_ui_data(UClass& root_class,
                       UClass& button_class,
                       UClass& main_class,
                       UClass& level_class,
                       UClass& pause_class,
                       UClass& completion_class) -> bool {
    auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return false;
    }
    ui_data->Modify();
    ui_data->widget_classes.classes.Add(ml::ioj::UGameUiRootLayout::StaticClass(), &root_class);
    ui_data->widget_classes.classes.Add(ml::ioj::UMenuButtonWidget::StaticClass(), &button_class);
    ui_data->widget_classes.classes.Add(ml::ioj::UMainMenuWidget::StaticClass(), &main_class);
    ui_data->widget_classes.classes.Add(ml::ioj::ULevelSelectWidget::StaticClass(), &level_class);
    ui_data->widget_classes.classes.Add(ml::ioj::UPauseMenuWidget::StaticClass(), &pause_class);
    ui_data->widget_classes.classes.Add(ml::ioj::ULevelCompletionWidget::StaticClass(),
                                        &completion_class);
    return save_asset(*ui_data);
}

auto generate_level_select_widget(UClass& button_class) -> UClass* {
    auto* const blueprint{LoadObject<UWidgetBlueprint>(nullptr, widget_object_path)};
    if (!IsValid(blueprint) || !IsValid(blueprint->WidgetTree)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load %s"), widget_object_path);
        return nullptr;
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
    page_slot->SetHorizontalAlignment(HAlign_Fill);
    page_slot->SetVerticalAlignment(VAlign_Fill);
    tree.RootWidget = root;

    auto* const heading{tree.ConstructWidget<UTextBlock>()};
    heading->SetText(FText::FromString(TEXT("Scripted Levels")));
    auto heading_font{heading->GetFont()};
    heading_font.Size = 28;
    heading->SetFont(heading_font);
    auto* const heading_slot{page->AddChildToVerticalBox(heading)};
    heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    auto* const body{tree.ConstructWidget<UHorizontalBox>()};
    auto* const body_slot{page->AddChildToVerticalBox(body)};
    body_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});

    auto* const controls{tree.ConstructWidget<UVerticalBox>()};
    auto* const controls_slot{body->AddChildToHorizontalBox(controls)};
    controls_slot->SetSize(FSlateChildSize{ESlateSizeRule::Automatic});
    controls_slot->SetPadding(FMargin{0.0f, 0.0f, 20.0f, 0.0f});

    auto* const controls_heading{tree.ConstructWidget<UTextBlock>()};
    controls_heading->SetText(FText::FromString(TEXT("Controls")));
    auto controls_heading_font{controls_heading->GetFont()};
    controls_heading_font.Size = 20;
    controls_heading->SetFont(controls_heading_font);
    auto* const controls_heading_slot{controls->AddChildToVerticalBox(controls_heading)};
    controls_heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    make_menu_button(tree, *controls, button_class, TEXT("launch_button"), TEXT("Launch"));
    make_menu_button(
        tree, *controls, button_class, TEXT("start_paused_button"), TEXT("Start Paused"));
    make_menu_button(tree, *controls, button_class, TEXT("refresh_button"), TEXT("Refresh"));
    make_menu_button(tree, *controls, button_class, TEXT("back_button"), TEXT("Back"));

    auto* const levels{tree.ConstructWidget<UVerticalBox>()};
    auto* const levels_slot{body->AddChildToHorizontalBox(levels)};
    levels_slot->SetSize(FSlateChildSize{ESlateSizeRule::Automatic});
    levels_slot->SetPadding(FMargin{0.0f, 0.0f, 24.0f, 0.0f});

    auto* const levels_heading{tree.ConstructWidget<UTextBlock>()};
    levels_heading->SetText(FText::FromString(TEXT("Levels")));
    auto levels_heading_font{levels_heading->GetFont()};
    levels_heading_font.Size = 20;
    levels_heading->SetFont(levels_heading_font);
    auto* const levels_heading_slot{levels->AddChildToVerticalBox(levels_heading)};
    levels_heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});

    auto* const scroll{tree.ConstructWidget<UScrollBox>()};
    auto* const level_list{make_widget<UVerticalBox>(tree, TEXT("level_list"))};
    scroll->AddChild(level_list);
    auto* const scroll_slot{levels->AddChildToVerticalBox(scroll)};
    scroll_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});

    auto* const details{tree.ConstructWidget<UVerticalBox>()};
    auto* const details_slot{body->AddChildToHorizontalBox(details)};
    details_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});

    auto* const title{make_widget<UTextBlock>(tree, TEXT("title_text"))};
    auto title_font{title->GetFont()};
    title_font.Size = 26;
    title->SetFont(title_font);
    auto* const title_slot{details->AddChildToVerticalBox(title)};
    title_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 4.0f});
    auto* const status{make_widget<UTextBlock>(tree, TEXT("status_text"))};
    status->SetAutoWrapText(true);
    auto* const status_slot{details->AddChildToVerticalBox(status)};
    status_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});
    auto* const description{make_widget<UTextBlock>(tree, TEXT("description_text"))};
    description->SetAutoWrapText(true);
    auto* const description_slot{details->AddChildToVerticalBox(description)};
    description_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 12.0f});
    auto* const selected_file{make_widget<UTextBlock>(tree, TEXT("selected_file_text"))};
    auto* const selected_file_slot{details->AddChildToVerticalBox(selected_file)};
    selected_file_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const details_text{make_widget<UTextBlock>(tree, TEXT("details_text"))};
    details_text->SetAutoWrapText(true);
    auto* const level_details_slot{details->AddChildToVerticalBox(details_text)};
    level_details_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 16.0f});
    auto* const script_heading{tree.ConstructWidget<UTextBlock>()};
    script_heading->SetText(FText::FromString(TEXT("Script")));
    auto script_heading_font{script_heading->GetFont()};
    script_heading_font.Size = 20;
    script_heading->SetFont(script_heading_font);
    auto* const script_heading_slot{details->AddChildToVerticalBox(script_heading)};
    script_heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});

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
        return nullptr;
    }
    return save_asset(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
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
    game_mode->DefaultPawnClass = nullptr;
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
    auto const input_generated{generate_menu_input_assets()};
    auto* const button_class{generate_menu_button_widget()};
    auto* const root_class{generate_root_layout_widget()};
    auto* const pause_class{IsValid(button_class) ? generate_pause_menu_widget(*button_class)
                                                  : nullptr};
    auto* const completion_class{
        IsValid(button_class) ? generate_level_completion_widget(*button_class) : nullptr};
    auto* const main_class{IsValid(button_class) ? generate_main_menu_widget(*button_class)
                                                 : nullptr};
    auto* const level_class{IsValid(button_class) ? generate_level_select_widget(*button_class)
                                                  : nullptr};
    auto const ui_generated{IsValid(root_class) && IsValid(button_class) && IsValid(main_class) &&
                            IsValid(level_class) && IsValid(pause_class) &&
                            IsValid(completion_class) &&
                            configure_ui_data(*root_class,
                                              *button_class,
                                              *main_class,
                                              *level_class,
                                              *pause_class,
                                              *completion_class)};
    auto const map_generated{generate_runtime_map()};
    return input_generated && ui_generated && map_generated ? 0 : 1;
}
