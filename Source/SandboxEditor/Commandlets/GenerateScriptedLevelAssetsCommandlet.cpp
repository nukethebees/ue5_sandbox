#include "SandboxEditor/Commandlets/GenerateScriptedLevelAssetsCommandlet.h"

#include <SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h>
#include <SpaceGame/input/ControlProfiles.h>
#include <SpaceGame/input/SpaceGameInputModifier.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuGameMode.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/ForceStatusWidget.h>
#include <SpaceGamePresentation/presentation/widgets/MissionStatusWidget.h>
#include <SpaceGamePresentation/presentation/widgets/TeamEntityTableWidget.h>
#include <SpaceGamePresentation/presentation/widgets/TopKillersWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>
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
#include <InputModifiers.h>
#include <InputTriggers.h>
#include <Kismet2/BlueprintEditorUtils.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <Misc/PackageName.h>
#include <PlayerMappableKeySettings.h>
#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <UObject/UnrealType.h>
#include <WidgetBlueprint.h>
#include <WidgetBlueprintOperationUtils.h>
#include <Widgets/CommonActivatableWidgetContainer.h>

namespace {
constexpr TCHAR widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect.WBP_LevelSelect")};
constexpr TCHAR widget_package_name[]{TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect")};
constexpr TCHAR main_menu_widget_object_path[]{
    TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu.WBP_MainMenu")};
constexpr TCHAR main_menu_widget_package_name[]{TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu")};
constexpr TCHAR pause_menu_widget_object_path[]{
    TEXT("/Game/UI/pause_menu/WBP_PauseMenu.WBP_PauseMenu")};
constexpr TCHAR pause_menu_widget_package_name[]{TEXT("/Game/UI/pause_menu/WBP_PauseMenu")};
constexpr TCHAR team_entity_table_class_path[]{
    TEXT("/Game/UI/ship_hud/WBP_TeamEntityTable.WBP_TeamEntityTable_C")};
constexpr TCHAR top_killers_class_path[]{TEXT("/Game/UI/ship_hud/WBP_TopKillers.WBP_TopKillers_C")};
constexpr TCHAR completion_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_LevelCompletion.WBP_LevelCompletion")};
constexpr TCHAR completion_widget_package_name[]{TEXT("/SpaceGame/UI/InGame/WBP_LevelCompletion")};
constexpr TCHAR menu_button_package_name[]{TEXT("/SpaceGame/UI/Common/WBP_MenuButton")};
constexpr TCHAR menu_button_object_path[]{
    TEXT("/SpaceGame/UI/Common/WBP_MenuButton.WBP_MenuButton")};
constexpr TCHAR root_layout_package_name[]{TEXT("/SpaceGame/UI/Common/WBP_GameUiRoot")};
constexpr TCHAR root_layout_object_path[]{
    TEXT("/SpaceGame/UI/Common/WBP_GameUiRoot.WBP_GameUiRoot")};
constexpr TCHAR save_game_viewer_object_path[]{
    TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameViewer.WBP_SaveGameViewer")};
constexpr TCHAR save_game_viewer_package_name[]{TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameViewer")};
constexpr TCHAR back_action_package_name[]{TEXT("/SpaceGame/Input/UI/IA_menu_back")};
constexpr TCHAR back_action_object_path[]{TEXT("/SpaceGame/Input/UI/IA_menu_back.IA_menu_back")};
constexpr TCHAR menu_mapping_package_name[]{TEXT("/SpaceGame/Input/UI/IMC_menu")};
constexpr TCHAR menu_mapping_object_path[]{TEXT("/SpaceGame/Input/UI/IMC_menu.IMC_menu")};
constexpr TCHAR global_mapping_object_path[]{
    TEXT("/SpaceGame/Input/Player/IMC_Player_Global.IMC_Player_Global")};
constexpr TCHAR pause_action_object_path[]{TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause")};
constexpr TCHAR battle_viewer_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BattleViewerHud.WBP_BattleViewerHud")};
constexpr TCHAR battle_viewer_widget_package_name[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BattleViewerHud")};
constexpr TCHAR benchmark_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BenchmarkHud.WBP_BenchmarkHud")};
constexpr TCHAR benchmark_widget_package_name[]{TEXT("/SpaceGame/UI/InGame/WBP_BenchmarkHud")};
constexpr TCHAR observer_input_package_path[]{TEXT("/SpaceGame/Input/Observer/")};
constexpr TCHAR benchmark_input_package_path[]{TEXT("/SpaceGame/Input/Benchmark/")};
constexpr TCHAR ship_input_package_path[]{TEXT("/SpaceGame/Input/SpaceShip/")};
constexpr TCHAR ship_base_mapping_object_path[]{
    TEXT("/SpaceGame/Input/SpaceShip/IMC_SpaceShip_Base.IMC_SpaceShip_Base")};
constexpr TCHAR ship_aim_move_mapping_object_path[]{
    TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_aim_move."
         "IMC_space_ship_twinstick_aim_move")};
constexpr TCHAR ship_move_aim_mapping_object_path[]{
    TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_move_aim."
         "IMC_space_ship_twinstick_move_aim")};
constexpr TCHAR ship_z_roll_aim_mapping_object_path[]{
    TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_z-roll_aim."
         "IMC_space_ship_twinstick_z-roll_aim")};
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
constexpr TCHAR main_menu_map_package_name[]{TEXT("/SpaceGame/Levels/MainMenu")};
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

template <typename T>
auto make_widget(UWidgetTree& tree, UClass& widget_class, FName const name) -> T* {
    auto* const widget{tree.ConstructWidget<T>(&widget_class, name)};
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
    auto* const team_table_class{
        LoadClass<UTeamEntityTableWidget>(nullptr, team_entity_table_class_path)};
    auto* const top_killer_table_class{
        LoadClass<UTopKillersWidget>(nullptr, top_killers_class_path)};
    if (!IsValid(team_table_class) || !IsValid(top_killer_table_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load pause-menu battle table classes"));
        return nullptr;
    }

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
    make_menu_button(tree, *actions, button_class, TEXT("forces_button"), TEXT("Forces"));
    make_menu_button(tree, *actions, button_class, TEXT("combat_button"), TEXT("Combat"));
    make_menu_button(tree, *actions, button_class, TEXT("telemetry_button"), TEXT("Telemetry"));
    make_menu_button(tree, *actions, button_class, TEXT("audio_button"), TEXT("Audio"));
    make_menu_button(tree, *actions, button_class, TEXT("controls_button"), TEXT("Controls"));
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

    auto make_scroll_page{[&tree, switcher](FName const scroll_name) {
        auto* const scroll{make_widget<UScrollBox>(tree, scroll_name)};
        auto* const contents{tree.ConstructWidget<UVerticalBox>()};
        auto* const contents_slot{CastChecked<UScrollBoxSlot>(scroll->AddChild(contents))};
        contents_slot->SetHorizontalAlignment(HAlign_Fill);
        switcher->AddChild(scroll);
        return contents;
    }};

    auto* const overview_contents{make_scroll_page(TEXT("overview_scroll"))};
    auto* const summary_heading{make_widget<UTextBlock>(tree, TEXT("overview_summary_heading"))};
    summary_heading->SetText(FText::FromString(TEXT("Level Summary")));
    overview_contents->AddChildToVerticalBox(summary_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});

    auto* const summary_panel{make_widget<UBorder>(tree, TEXT("overview_summary_panel"))};
    auto* const summary_grid{tree.ConstructWidget<UGridPanel>()};
    summary_grid->SetColumnFill(0, 0.7f);
    summary_grid->SetColumnFill(1, 0.3f);
    add_pause_stat_row(tree,
                       *summary_grid,
                       0,
                       TEXT("overview_label_elapsed_time"),
                       FText::FromString(TEXT("Elapsed Time")),
                       TEXT("elapsed_time_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       1,
                       TEXT("overview_label_entities_spawned"),
                       FText::FromString(TEXT("Entities Spawned")),
                       TEXT("entities_spawned_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       2,
                       TEXT("overview_label_entities_active"),
                       FText::FromString(TEXT("Entities Active")),
                       TEXT("entities_active_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       3,
                       TEXT("overview_label_entities_destroyed"),
                       FText::FromString(TEXT("Entities Destroyed")),
                       TEXT("entities_destroyed_value"));
    add_pause_stat_row(tree,
                       *summary_grid,
                       4,
                       TEXT("overview_label_kills"),
                       FText::FromString(TEXT("Kills")),
                       TEXT("kills_value"));
    summary_panel->SetContent(summary_grid);
    overview_contents->AddChildToVerticalBox(summary_panel)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});

    auto* const forces_contents{make_scroll_page(TEXT("forces_scroll"))};
    auto* const forces_heading{make_widget<UTextBlock>(tree, TEXT("forces_counts_heading"))};
    forces_heading->SetText(FText::FromString(TEXT("Alive Entity Counts")));
    forces_contents->AddChildToVerticalBox(forces_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const forces_table{
        make_widget<UTeamEntityTableWidget>(tree, *team_table_class, TEXT("forces_table"))};
    forces_contents->AddChildToVerticalBox(forces_table)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});

    auto* const combat_contents{make_scroll_page(TEXT("combat_scroll"))};
    auto* const top_killers_heading{
        make_widget<UTextBlock>(tree, TEXT("combat_top_killers_heading"))};
    top_killers_heading->SetText(FText::FromString(TEXT("Top Killers")));
    combat_contents->AddChildToVerticalBox(top_killers_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const top_killers_table{make_widget<UTopKillersWidget>(
        tree, *top_killer_table_class, TEXT("combat_top_killers_table"))};
    combat_contents->AddChildToVerticalBox(top_killers_table)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});
    auto* const team_kills_heading{
        make_widget<UTextBlock>(tree, TEXT("combat_team_kills_heading"))};
    team_kills_heading->SetText(FText::FromString(TEXT("Team Kills by Victim Type")));
    combat_contents->AddChildToVerticalBox(team_kills_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const team_kills_table{make_widget<UTeamEntityTableWidget>(
        tree, *team_table_class, TEXT("combat_team_kills_table"))};
    combat_contents->AddChildToVerticalBox(team_kills_table)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});

    auto* const telemetry_contents{make_scroll_page(TEXT("telemetry_scroll"))};
    auto* const telemetry_summary_heading{
        make_widget<UTextBlock>(tree, TEXT("telemetry_summary_heading"))};
    telemetry_summary_heading->SetText(FText::FromString(TEXT("Weapon Activity")));
    telemetry_contents->AddChildToVerticalBox(telemetry_summary_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const telemetry_summary_panel{
        make_widget<UBorder>(tree, TEXT("telemetry_summary_panel"))};
    auto* const telemetry_summary_grid{tree.ConstructWidget<UGridPanel>()};
    telemetry_summary_grid->SetColumnFill(0, 0.7f);
    telemetry_summary_grid->SetColumnFill(1, 0.3f);
    add_pause_stat_row(tree,
                       *telemetry_summary_grid,
                       0,
                       TEXT("telemetry_label_lasers_fired"),
                       FText::FromString(TEXT("Lasers Fired")),
                       TEXT("lasers_fired_value"));
    add_pause_stat_row(tree,
                       *telemetry_summary_grid,
                       1,
                       TEXT("telemetry_label_lasers_active"),
                       FText::FromString(TEXT("Lasers Active")),
                       TEXT("lasers_active_value"));
    telemetry_summary_panel->SetContent(telemetry_summary_grid);
    telemetry_contents->AddChildToVerticalBox(telemetry_summary_panel)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 20.0f});

    auto* const graph_heading{make_widget<UTextBlock>(tree, TEXT("telemetry_graph_heading"))};
    graph_heading->SetText(FText::FromString(TEXT("Entity Activity")));
    telemetry_contents->AddChildToVerticalBox(graph_heading)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 4.0f});
    auto* const graph_description{
        make_widget<UTextBlock>(tree, TEXT("telemetry_graph_description"))};
    graph_description->SetText(
        FText::FromString(TEXT("Active entities and kills over simulation time (seconds).")));
    graph_description->SetAutoWrapText(true);
    telemetry_contents->AddChildToVerticalBox(graph_description)
        ->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    auto* const graph_size{tree.ConstructWidget<USizeBox>()};
    graph_size->SetHeightOverride(260.0f);
    auto* const graph_host{make_widget<UNativeWidgetHost>(tree, TEXT("telemetry_graph_host"))};
    graph_size->SetContent(graph_host);
    telemetry_contents->AddChildToVerticalBox(graph_size)->SetHorizontalAlignment(HAlign_Fill);

    auto* const options{make_widget<ml::ioj::UOptionsWidget>(tree, TEXT("options_widget"))};
    switcher->AddChild(options);

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_level_completion_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(completion_widget_object_path,
                                        completion_widget_package_name,
                                        TEXT("WBP_LevelCompletion"),
                                        *ml::ioj::ULevelCompletionWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    tree.RootWidget = make_widget<UNativeWidgetHost>(tree, TEXT("view_host"));
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_main_menu_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(main_menu_widget_object_path,
                                        main_menu_widget_package_name,
                                        TEXT("WBP_MainMenu"),
                                        *ml::ioj::UMainMenuWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    blueprint->WidgetTree->RootWidget = nullptr;
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

auto create_input_action(TCHAR const* const package_path,
                         TCHAR const* const asset_name,
                         EInputActionValueType const value_type) -> UInputAction* {
    auto const package_name{FString::Printf(TEXT("%s%s"), package_path, asset_name)};
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, asset_name)};
    auto* const action{
        load_or_create_asset<UInputAction>(*object_path, *package_name, FName{asset_name})};
    if (IsValid(action)) {
        action->Modify();
        action->ValueType = value_type;
    }
    return action;
}

void add_negate_modifier(FEnhancedActionKeyMapping& mapping, UInputMappingContext& outer) {
    mapping.Modifiers.Add(NewObject<UInputModifierNegate>(&outer));
}

void add_swizzle_modifier(FEnhancedActionKeyMapping& mapping, UInputMappingContext& outer) {
    mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(&outer));
}

auto generate_control_context_input_assets(FObserverControlInputs& observer,
                                           FBenchmarkControlInputs& benchmark) -> bool {
    observer.move = create_input_action(
        observer_input_package_path, TEXT("IA_ObserverMove"), EInputActionValueType::Axis2D);
    observer.vertical_move = create_input_action(observer_input_package_path,
                                                 TEXT("IA_ObserverVerticalMove"),
                                                 EInputActionValueType::Axis1D);
    observer.look = create_input_action(
        observer_input_package_path, TEXT("IA_ObserverLook"), EInputActionValueType::Axis2D);
    observer.engage_look = create_input_action(
        observer_input_package_path, TEXT("IA_ObserverEngageLook"), EInputActionValueType::Boolean);
    observer.adjust_speed = create_input_action(
        observer_input_package_path, TEXT("IA_ObserverAdjustSpeed"), EInputActionValueType::Axis1D);
    observer.boost = create_input_action(
        observer_input_package_path, TEXT("IA_ObserverBoost"), EInputActionValueType::Boolean);

    auto const observer_mapping_package{
        FString::Printf(TEXT("%s%s"), observer_input_package_path, TEXT("IMC_Observer"))};
    auto const observer_mapping_object{
        FString::Printf(TEXT("%s.%s"), *observer_mapping_package, TEXT("IMC_Observer"))};
    observer.mapping_context = load_or_create_asset<UInputMappingContext>(
        *observer_mapping_object, *observer_mapping_package, TEXT("IMC_Observer"));

    benchmark.exit = create_input_action(
        benchmark_input_package_path, TEXT("IA_ExitBenchmark"), EInputActionValueType::Boolean);
    auto const benchmark_mapping_package{
        FString::Printf(TEXT("%s%s"), benchmark_input_package_path, TEXT("IMC_Benchmark"))};
    auto const benchmark_mapping_object{
        FString::Printf(TEXT("%s.%s"), *benchmark_mapping_package, TEXT("IMC_Benchmark"))};
    benchmark.mapping_context = load_or_create_asset<UInputMappingContext>(
        *benchmark_mapping_object, *benchmark_mapping_package, TEXT("IMC_Benchmark"));

    if (!observer.is_valid() || !benchmark.is_valid()) {
        UE_LOG(LogTemp, Error, TEXT("Could not load or create control-context input assets"));
        return false;
    }

    observer.mapping_context->Modify();
    observer.mapping_context->UnmapAll();
    observer.mapping_context->MapKey(observer.move, EKeys::D);
    auto& move_left{observer.mapping_context->MapKey(observer.move, EKeys::A)};
    add_negate_modifier(move_left, *observer.mapping_context);
    auto& move_forward{observer.mapping_context->MapKey(observer.move, EKeys::W)};
    add_swizzle_modifier(move_forward, *observer.mapping_context);
    auto& move_backward{observer.mapping_context->MapKey(observer.move, EKeys::S)};
    add_swizzle_modifier(move_backward, *observer.mapping_context);
    add_negate_modifier(move_backward, *observer.mapping_context);
    observer.mapping_context->MapKey(observer.vertical_move, EKeys::E);
    auto& move_down{observer.mapping_context->MapKey(observer.vertical_move, EKeys::Q)};
    add_negate_modifier(move_down, *observer.mapping_context);
    observer.mapping_context->MapKey(observer.look, EKeys::Mouse2D);
    observer.mapping_context->MapKey(observer.engage_look, EKeys::RightMouseButton);
    observer.mapping_context->MapKey(observer.adjust_speed, EKeys::MouseWheelAxis);
    observer.mapping_context->MapKey(observer.boost, EKeys::LeftShift);

    benchmark.mapping_context->Modify();
    benchmark.mapping_context->UnmapAll();
    benchmark.mapping_context->MapKey(benchmark.exit, EKeys::Escape);

    TArray<UObject*> const assets{observer.move,
                                  observer.vertical_move,
                                  observer.look,
                                  observer.engage_look,
                                  observer.adjust_speed,
                                  observer.boost,
                                  observer.mapping_context,
                                  benchmark.exit,
                                  benchmark.mapping_context};
    for (auto* const asset : assets) {
        if (!save_asset(*asset)) {
            return false;
        }
    }
    return true;
}

auto generate_battle_viewer_widget() -> UClass* {
    auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return nullptr;
    }
    auto const mission_status_class{ui_data->get_widget_class<UMissionStatusWidget>()};
    auto const value_class{ui_data->get_widget_class<UValueWidget>()};
    if (!IsValid(mission_status_class) || !IsValid(value_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load Battle Viewer child widget classes"));
        return nullptr;
    }

    auto* const blueprint{load_or_create_widget_blueprint(battle_viewer_widget_object_path,
                                                          battle_viewer_widget_package_name,
                                                          TEXT("WBP_BattleViewerHud"),
                                                          *UBattleViewerHudWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("battle_viewer_root"))};

    auto* const force_status{make_widget<UForceStatusWidget>(tree, TEXT("force_status_widget"))};
    auto* const force_slot{root->AddChildToOverlay(force_status)};
    force_slot->SetHorizontalAlignment(HAlign_Left);
    force_slot->SetVerticalAlignment(VAlign_Top);
    force_slot->SetPadding(FMargin{24.0f});

    auto* const mission_status{make_widget<UMissionStatusWidget>(
        tree, *mission_status_class.Get(), TEXT("mission_status_panel"))};
    auto* const mission_slot{root->AddChildToOverlay(mission_status)};
    mission_slot->SetHorizontalAlignment(HAlign_Right);
    mission_slot->SetVerticalAlignment(VAlign_Top);
    mission_slot->SetPadding(FMargin{24.0f});

    auto* const controls{make_widget<UVerticalBox>(tree, TEXT("observer_controls"))};
    auto* const controls_slot{root->AddChildToOverlay(controls)};
    controls_slot->SetHorizontalAlignment(HAlign_Center);
    controls_slot->SetVerticalAlignment(VAlign_Top);
    controls_slot->SetPadding(FMargin{24.0f});

    auto* const camera_speed{
        make_widget<UValueWidget>(tree, *value_class.Get(), TEXT("camera_speed_widget"))};
    controls->AddChildToVerticalBox(camera_speed);
    auto* const controls_label{make_widget<UTextBlock>(tree, TEXT("controls_label"))};
    controls_label->SetAutoWrapText(true);
    controls->AddChildToVerticalBox(controls_label)->SetPadding(FMargin{0.0f, 4.0f, 0.0f, 0.0f});

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_benchmark_widget(UClass& button_class) -> UClass* {
    auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return nullptr;
    }
    auto const value_class{ui_data->get_widget_class<UValueWidget>()};
    if (!IsValid(value_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load Benchmark HUD value widget class"));
        return nullptr;
    }

    auto* const blueprint{load_or_create_widget_blueprint(benchmark_widget_object_path,
                                                          benchmark_widget_package_name,
                                                          TEXT("WBP_BenchmarkHud"),
                                                          *UBenchmarkHudWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("benchmark_root"))};
    auto* const panel{make_widget<UBorder>(tree, TEXT("benchmark_panel"))};
    panel->SetPadding(FMargin{18.0f});
    auto* const panel_slot{root->AddChildToOverlay(panel)};
    panel_slot->SetHorizontalAlignment(HAlign_Right);
    panel_slot->SetVerticalAlignment(VAlign_Top);
    panel_slot->SetPadding(FMargin{24.0f});

    auto* const content{make_widget<UVerticalBox>(tree, TEXT("benchmark_content"))};
    panel->AddChild(content);
    auto* const title{make_widget<UTextBlock>(tree, TEXT("benchmark_title"))};
    content->AddChildToVerticalBox(title);
    auto* const ticks{
        make_widget<UValueWidget>(tree, *value_class.Get(), TEXT("ticks_remaining_widget"))};
    content->AddChildToVerticalBox(ticks)->SetPadding(FMargin{0.0f, 8.0f, 0.0f, 12.0f});
    make_menu_button(
        tree, *content, button_class, TEXT("end_benchmark_button"), TEXT("End Benchmark"));

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto input_category(UInputAction const& action) -> FText {
    auto const name{action.GetName().ToLower()};
    if (name.Contains(TEXT("fire"))) {
        return NSLOCTEXT("Controls", "CombatCategory", "Combat");
    }
    if (name.Contains(TEXT("cycle")) || name.Contains(TEXT("sample"))) {
        return NSLOCTEXT("Controls", "UtilityCategory", "Utility");
    }
    return NSLOCTEXT("Controls", "FlightCategory", "Flight");
}

auto action_display_name(UInputAction const& action) -> FString {
    auto result{action.GetName()};
    result.RemoveFromStart(TEXT("IA_"));
    result.RemoveFromStart(TEXT("ship_"));
    result.ReplaceInline(TEXT("_"), TEXT(" "));
    result = result.ToLower();
    if (!result.IsEmpty()) {
        result[0] = FChar::ToUpper(result[0]);
    }
    return result;
}

auto mapping_device_is_gamepad(FEnhancedActionKeyMapping const& mapping) -> bool {
    return mapping.Key.IsGamepadKey();
}

auto mapping_device_is_mouse(FEnhancedActionKeyMapping const& mapping) -> bool {
    return mapping.Key.IsMouseButton();
}

void duplicate_instanced_mapping_data(FEnhancedActionKeyMapping& mapping,
                                      UInputMappingContext& destination) {
    for (auto& modifier : mapping.Modifiers) {
        modifier =
            IsValid(modifier) ? DuplicateObject<UInputModifier>(modifier, &destination) : nullptr;
    }
    for (auto& trigger : mapping.Triggers) {
        trigger =
            IsValid(trigger) ? DuplicateObject<UInputTrigger>(trigger, &destination) : nullptr;
    }
}

void configure_mapping(FEnhancedActionKeyMapping& mapping,
                       UInputMappingContext& owner,
                       int32 const same_device_action_count,
                       int32 const same_device_action_index) {
    if (!IsValid(mapping.Action)) {
        return;
    }

    auto* const behavior_property{FindFProperty<FEnumProperty>(
        FEnhancedActionKeyMapping::StaticStruct(), TEXT("SettingBehavior"))};
    auto* const settings_property{FindFProperty<FObjectProperty>(
        FEnhancedActionKeyMapping::StaticStruct(), TEXT("PlayerMappableKeySettings"))};
    check(behavior_property != nullptr && settings_property != nullptr);
    behavior_property->GetUnderlyingProperty()->SetIntPropertyValue(
        behavior_property->ContainerPtrToValuePtr<void>(&mapping),
        static_cast<int64>(EPlayerMappableKeySettingBehaviors::OverrideSettings));
    auto* const settings{NewObject<UPlayerMappableKeySettings>(&owner)};
    auto mapping_name{mapping.Action->GetFName().ToString()};
    if (same_device_action_count > 1) {
        mapping_name += FString::Printf(TEXT(".%d"), same_device_action_index + 1);
    }
    settings->Name = FName{mapping_name};

    auto display_name{action_display_name(*mapping.Action)};
    if (same_device_action_count > 1) {
        display_name += FString::Printf(TEXT(" — %s"), *mapping.Key.GetDisplayName().ToString());
    }
    settings->DisplayName = FText::FromString(display_name);
    settings->DisplayCategory = input_category(*mapping.Action);
    settings_property->SetObjectPropertyValue_InContainer(&mapping, settings);

    auto const analog{mapping.Key.IsAxis1D() || mapping.Key.IsAxis2D() || mapping.Key.IsAxis3D()};
    if (!analog) {
        return;
    }
    mapping.Modifiers.RemoveAll([](TObjectPtr<UInputModifier> const& modifier) {
        return IsValid(modifier) && (modifier->IsA<UInputModifierDeadZone>() ||
                                     modifier->IsA<ml::ioj::USpaceGameInputModifier>());
    });

    auto* const response_modifier{NewObject<ml::ioj::USpaceGameInputModifier>(&owner)};
    auto const action_name{mapping.Action->GetName()};
    if (mapping_device_is_mouse(mapping)) {
        if (!action_name.Contains(TEXT("Turn"), ESearchCase::IgnoreCase)) {
            return;
        }
        response_modifier->response = ml::ioj::ESpaceGameInputResponse::TurnPointerDelta;
    } else if (!mapping_device_is_gamepad(mapping)) {
        return;
    } else if (action_name.Contains(TEXT("Turn"), ESearchCase::IgnoreCase)) {
        response_modifier->response = ml::ioj::ESpaceGameInputResponse::GamepadTurn;
    } else {
        response_modifier->response = ml::ioj::ESpaceGameInputResponse::GamepadMove;
    }
    mapping.Modifiers.Add(response_modifier);
}

void configure_mappings(TArray<FEnhancedActionKeyMapping>& mappings,
                        UInputMappingContext& owner,
                        UInputAction& turn_pointer_delta_action) {
    for (auto& mapping : mappings) {
        if (mapping.Key.IsAxis2D() && mapping_device_is_mouse(mapping) && IsValid(mapping.Action) &&
            mapping.Action->GetName().Contains(TEXT("Turn"), ESearchCase::IgnoreCase)) {
            mapping.Action = &turn_pointer_delta_action;
            mapping.Triggers.RemoveAll([](TObjectPtr<UInputTrigger> const& trigger) {
                return IsValid(trigger) && trigger->IsA<UInputTriggerChordAction>();
            });
        }
    }

    auto mapping_group = [](FEnhancedActionKeyMapping const& mapping) {
        return FString::Printf(
            TEXT("%s:%s"),
            IsValid(mapping.Action) ? *mapping.Action->GetPathName() : TEXT("Invalid"),
            mapping_device_is_gamepad(mapping) ? TEXT("Gamepad") : TEXT("KeyboardMouse"));
    };
    TMap<FString, int32> counts;
    for (auto const& mapping : mappings) {
        counts.FindOrAdd(mapping_group(mapping))++;
    }

    TMap<FString, int32> indices;
    for (auto& mapping : mappings) {
        auto const key{mapping_group(mapping)};
        auto& index{indices.FindOrAdd(key)};
        configure_mapping(mapping, owner, counts.FindRef(key), index);
        ++index;
    }
}

auto set_profile_override(UInputMappingContext& destination,
                          FString const& profile_id,
                          UInputMappingContext const& source,
                          TConstArrayView<FEnhancedActionKeyMapping> const shared_mappings,
                          UInputAction& turn_pointer_delta_action) -> bool {
    auto* const property{FindFProperty<FMapProperty>(UInputMappingContext::StaticClass(),
                                                     TEXT("MappingProfileOverrides"))};
    if (property == nullptr) {
        UE_LOG(LogTemp, Error, TEXT("Could not find MappingProfileOverrides property"));
        return false;
    }

    FScriptMapHelper mappings{property, property->ContainerPtrToValuePtr<void>(&destination)};
    auto* value{mappings.FindValueFromHash(&profile_id)};
    if (value == nullptr) {
        auto const index{mappings.AddDefaultValue_Invalid_NeedsRehash()};
        CastFieldChecked<FStrProperty>(property->KeyProp)
            ->SetPropertyValue(mappings.GetKeyPtr(index), profile_id);
        mappings.Rehash();
        value = mappings.FindValueFromHash(&profile_id);
    }

    auto* const data{reinterpret_cast<FInputMappingContextMappingData*>(value)};
    data->Mappings = source.GetMappings();
    data->Mappings.RemoveAll([](FEnhancedActionKeyMapping const& mapping) {
        return !mapping_device_is_gamepad(mapping);
    });
    for (auto const& mapping : shared_mappings) {
        if (!mapping_device_is_gamepad(mapping)) {
            data->Mappings.Add(mapping);
        }
    }
    for (auto& mapping : data->Mappings) {
        duplicate_instanced_mapping_data(mapping, destination);
    }
    configure_mappings(data->Mappings, destination, turn_pointer_delta_action);
    return true;
}

struct FGeneratedShipTurnActions {
    UInputAction* pointer_delta{nullptr};
    UInputAction* engage_pointer{nullptr};

    auto is_valid() const -> bool { return IsValid(pointer_delta) && IsValid(engage_pointer); }
};

auto generate_gameplay_input_assets() -> FGeneratedShipTurnActions {
    FGeneratedShipTurnActions actions{
        .pointer_delta = create_input_action(ship_input_package_path,
                                             TEXT("IA_Ship_TurnPointerDelta"),
                                             EInputActionValueType::Axis2D),
        .engage_pointer = create_input_action(ship_input_package_path,
                                              TEXT("IA_Ship_EngagePointerTurn"),
                                              EInputActionValueType::Boolean),
    };
    auto* const base{LoadObject<UInputMappingContext>(nullptr, ship_base_mapping_object_path)};
    auto* const aim_move{
        LoadObject<UInputMappingContext>(nullptr, ship_aim_move_mapping_object_path)};
    auto* const move_aim{
        LoadObject<UInputMappingContext>(nullptr, ship_move_aim_mapping_object_path)};
    auto* const z_roll_aim{
        LoadObject<UInputMappingContext>(nullptr, ship_z_roll_aim_mapping_object_path)};
    if (!actions.is_valid() || !IsValid(base) || !IsValid(aim_move) || !IsValid(move_aim) ||
        !IsValid(z_roll_aim)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load ship input mapping contexts"));
        return {};
    }

    base->Modify();
    base->UnmapKey(actions.engage_pointer, EKeys::RightMouseButton);
    base->MapKey(actions.engage_pointer, EKeys::RightMouseButton);
    auto& default_mappings{const_cast<TArray<FEnhancedActionKeyMapping>&>(base->GetMappings())};
    configure_mappings(default_mappings, *base, *actions.pointer_delta);

    auto const profiles{ml::ioj::control_profile_definitions()};
    auto const success{
        set_profile_override(
            *base, profiles[1].id, *aim_move, default_mappings, *actions.pointer_delta) &&
        set_profile_override(
            *base, profiles[2].id, *move_aim, default_mappings, *actions.pointer_delta) &&
        set_profile_override(
            *base, profiles[3].id, *z_roll_aim, default_mappings, *actions.pointer_delta)};
    return success && save_asset(*actions.pointer_delta) && save_asset(*actions.engage_pointer) &&
                   save_asset(*base)
             ? actions
             : FGeneratedShipTurnActions{};
}

auto configure_ui_data(UClass& root_class,
                       UClass& button_class,
                       UClass& main_class,
                       UClass& level_class,
                       UClass& pause_class,
                       UClass& completion_class,
                       UClass& battle_viewer_class,
                       UClass& benchmark_class) -> bool {
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
    ui_data->widget_classes.classes.Add(UBattleViewerHudWidget::StaticClass(),
                                        &battle_viewer_class);
    ui_data->widget_classes.classes.Add(UBenchmarkHudWidget::StaticClass(), &benchmark_class);
    return save_asset(*ui_data);
}

auto generate_level_select_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(widget_object_path,
                                        widget_package_name,
                                        TEXT("WBP_LevelSelect"),
                                        *ml::s7::UScriptLevelSelectWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    blueprint->WidgetTree->RootWidget = nullptr;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_save_game_viewer_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(save_game_viewer_object_path,
                                        save_game_viewer_package_name,
                                        TEXT("WBP_SaveGameViewer"),
                                        *ml::ioj::USaveGameViewerWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    blueprint->WidgetTree->RootWidget = nullptr;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto configure_control_context_inputs(UBlueprint& blueprint,
                                      FObserverControlInputs const& observer,
                                      FBenchmarkControlInputs const& benchmark) -> bool {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(blueprint.GeneratedClass->GetDefaultObject())};
    auto* const observer_property{
        FindFProperty<FStructProperty>(blueprint.GeneratedClass, TEXT("observer_input"))};
    auto* const benchmark_property{
        FindFProperty<FStructProperty>(blueprint.GeneratedClass, TEXT("benchmark_input"))};
    if (!IsValid(controller) || !observer_property || !benchmark_property) {
        UE_LOG(LogTemp, Error, TEXT("Could not configure player controller context inputs"));
        return false;
    }

    controller->Modify();
    observer_property->CopyCompleteValue(
        observer_property->ContainerPtrToValuePtr<void>(controller), &observer);
    benchmark_property->CopyCompleteValue(
        benchmark_property->ContainerPtrToValuePtr<void>(controller), &benchmark);
    FPropertyChangedEvent property_changed{observer_property, EPropertyChangeType::ValueSet};
    controller->PostEditChangeProperty(property_changed);
    FBlueprintEditorUtils::MarkBlueprintAsModified(&blueprint);
    return true;
}

auto configure_gameplay_inputs(UBlueprint& blueprint,
                               UBlueprint const& source,
                               UInputMappingContext& mapping_context,
                               FGeneratedShipTurnActions const& turn_actions) -> bool {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(blueprint.GeneratedClass->GetDefaultObject())};
    auto const* const source_controller{
        Cast<ASpaceGamePlayerController>(source.GeneratedClass->GetDefaultObject())};
    auto* const input_property{
        FindFProperty<FStructProperty>(blueprint.GeneratedClass, TEXT("input"))};
    auto const* const source_input_property{
        FindFProperty<FStructProperty>(source.GeneratedClass, TEXT("input"))};
    auto* const global_input_property{
        FindFProperty<FStructProperty>(blueprint.GeneratedClass, TEXT("global_input"))};
    auto const* const source_global_input_property{
        FindFProperty<FStructProperty>(source.GeneratedClass, TEXT("global_input"))};
    if (!IsValid(controller) || !IsValid(source_controller) || !input_property ||
        !source_input_property || !global_input_property || !source_global_input_property) {
        UE_LOG(LogTemp, Error, TEXT("Could not configure player controller gameplay inputs"));
        return false;
    }

    controller->Modify();
    input_property->CopyCompleteValue(
        input_property->ContainerPtrToValuePtr<void>(controller),
        source_input_property->ContainerPtrToValuePtr<void>(source_controller));
    global_input_property->CopyCompleteValue(
        global_input_property->ContainerPtrToValuePtr<void>(controller),
        source_global_input_property->ContainerPtrToValuePtr<void>(source_controller));
    auto* const input{
        input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(controller)};
    input->mapping_context = &mapping_context;
    input->turn_pointer_delta = turn_actions.pointer_delta;
    input->engage_pointer_turn = turn_actions.engage_pointer;
    FPropertyChangedEvent property_changed{input_property, EPropertyChangeType::ValueSet};
    controller->PostEditChangeProperty(property_changed);
    blueprint.Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(&blueprint);
    return true;
}

auto load_or_create_player_controller(FObserverControlInputs const& observer,
                                      FBenchmarkControlInputs const& benchmark,
                                      FGeneratedShipTurnActions const& turn_actions)
    -> UBlueprint* {
    auto* const source{LoadObject<UBlueprint>(nullptr, source_player_controller_object_path)};
    auto* const mapping_context{
        LoadObject<UInputMappingContext>(nullptr, ship_base_mapping_object_path)};
    if (!IsValid(source) || !IsValid(source->GeneratedClass) || !IsValid(mapping_context)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load source player controller inputs"));
        return nullptr;
    }

    auto* blueprint{LoadObject<UBlueprint>(nullptr, player_controller_object_path)};
    if (!IsValid(blueprint)) {
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
    if (!configure_control_context_inputs(*blueprint, observer, benchmark) ||
        !configure_gameplay_inputs(*blueprint, *source, *mapping_context, turn_actions)) {
        return nullptr;
    }

    FKismetEditorUtilities::CompileBlueprint(blueprint);
    if (blueprint->Status == BS_Error || !IsValid(blueprint->GeneratedClass)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("BP_SpaceGamePlayerController failed to compile after input migration"));
        return nullptr;
    }
    if (!configure_control_context_inputs(*blueprint, observer, benchmark) ||
        !configure_gameplay_inputs(*blueprint, *source, *mapping_context, turn_actions)) {
        return nullptr;
    }
    auto* const compiled_controller{
        Cast<ASpaceGamePlayerController>(blueprint->GeneratedClass->GetDefaultObject())};
    auto const* const compiled_observer_property{
        FindFProperty<FStructProperty>(blueprint->GeneratedClass, TEXT("observer_input"))};
    auto const* const compiled_benchmark_property{
        FindFProperty<FStructProperty>(blueprint->GeneratedClass, TEXT("benchmark_input"))};
    auto const* const compiled_observer{
        IsValid(compiled_controller) && compiled_observer_property
            ? compiled_observer_property->ContainerPtrToValuePtr<FObserverControlInputs>(
                  compiled_controller)
            : nullptr};
    auto const* const compiled_benchmark{
        IsValid(compiled_controller) && compiled_benchmark_property
            ? compiled_benchmark_property->ContainerPtrToValuePtr<FBenchmarkControlInputs>(
                  compiled_controller)
            : nullptr};
    if (!compiled_observer || !compiled_observer->is_valid() || !compiled_benchmark ||
        !compiled_benchmark->is_valid()) {
        UE_LOG(LogTemp, Error, TEXT("Player controller context inputs did not compile"));
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
    auto* const source{LoadObject<USpaceGameLevelConfig>(nullptr, source_config_object_path)};
    if (!IsValid(source)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load source level config"));
        return nullptr;
    }

    auto* config{LoadObject<USpaceGameLevelConfig>(nullptr, *object_path)};
    if (!IsValid(config)) {
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
    for (TFieldIterator<FProperty> property{USpaceGameLevelConfig::StaticClass(),
                                            EFieldIteratorFlags::ExcludeSuper};
         property;
         ++property) {
        property->CopyCompleteValue_InContainer(config, source);
    }
    config->classes.player_controller_class = &player_controller_class;
    return save_asset(*config) ? config : nullptr;
}

auto generate_runtime_map(FObserverControlInputs const& observer,
                          FBenchmarkControlInputs const& benchmark,
                          FGeneratedShipTurnActions const& turn_actions) -> bool {
    auto* const controller_blueprint{
        load_or_create_player_controller(observer, benchmark, turn_actions)};
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

auto generate_main_menu_map() -> bool {
    auto* const world{UEditorLoadingAndSavingUtils::NewBlankMap(false)};
    if (!IsValid(world)) {
        UE_LOG(LogTemp, Error, TEXT("Could not create blank main-menu map"));
        return false;
    }

    auto* const world_settings{world->GetWorldSettings()};
    if (!IsValid(world_settings)) {
        UE_LOG(LogTemp, Error, TEXT("Could not configure the main-menu game mode"));
        return false;
    }
    world_settings->DefaultGameMode = ml::ioj::AMainMenuGameMode::StaticClass();

    auto const filename{FPackageName::LongPackageNameToFilename(
        main_menu_map_package_name, FPackageName::GetMapPackageExtension())};
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
    FObserverControlInputs observer_input;
    FBenchmarkControlInputs benchmark_input;
    auto const context_input_generated{
        generate_menu_input_assets() &&
        generate_control_context_input_assets(observer_input, benchmark_input)};
    auto const turn_actions{context_input_generated ? generate_gameplay_input_assets()
                                                    : FGeneratedShipTurnActions{}};
    auto const input_generated{turn_actions.is_valid()};
    auto* const button_class{generate_menu_button_widget()};
    auto* const root_class{generate_root_layout_widget()};
    auto* const pause_class{IsValid(button_class) ? generate_pause_menu_widget(*button_class)
                                                  : nullptr};
    auto* const completion_class{generate_level_completion_widget()};
    auto const save_viewer_generated{IsValid(generate_save_game_viewer_widget())};
    auto* const main_class{generate_main_menu_widget()};
    auto* const level_class{generate_level_select_widget()};
    auto* const battle_viewer_class{generate_battle_viewer_widget()};
    auto* const benchmark_class{IsValid(button_class) ? generate_benchmark_widget(*button_class)
                                                      : nullptr};
    auto const ui_generated{save_viewer_generated && IsValid(root_class) && IsValid(button_class) &&
                            IsValid(main_class) && IsValid(level_class) && IsValid(pause_class) &&
                            IsValid(completion_class) && IsValid(battle_viewer_class) &&
                            IsValid(benchmark_class) &&
                            configure_ui_data(*root_class,
                                              *button_class,
                                              *main_class,
                                              *level_class,
                                              *pause_class,
                                              *completion_class,
                                              *battle_viewer_class,
                                              *benchmark_class)};
    auto const map_generated{input_generated &&
                             generate_runtime_map(observer_input, benchmark_input, turn_actions) &&
                             generate_main_menu_map()};
    return input_generated && ui_generated && map_generated ? 0 : 1;
}
