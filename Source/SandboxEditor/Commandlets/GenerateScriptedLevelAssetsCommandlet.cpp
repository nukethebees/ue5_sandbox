#include "SandboxEditor/Commandlets/GenerateScriptedLevelAssetsCommandlet.h"

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
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
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <SandboxShaders/GpuStarfield/GpuStarfieldActor.h>

#include <AssetRegistry/AssetRegistryModule.h>
#include <Blueprint/WidgetTree.h>
#include <BlueprintEditorLibrary.h>
#include <CommonTextBlock.h>
#include <Components/Border.h>
#include <Components/Button.h>
#include <Components/ButtonSlot.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/NativeWidgetHost.h>
#include <Components/Overlay.h>
#include <Components/OverlaySlot.h>
#include <Components/SizeBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>
#include <EdGraph/EdGraph.h>
#include <EdGraph/EdGraphNode.h>
#include <Engine/Blueprint.h>
#include <Engine/BlueprintGeneratedClass.h>
#include <Engine/InputDelegateBinding.h>
#include <Engine/SimpleConstructionScript.h>
#include <FileHelpers.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <Kismet2/BlueprintEditorUtils.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <Misc/PackageName.h>
#include <PlayerMappableKeySettings.h>
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
constexpr TCHAR battle_viewer_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BattleViewerHud.WBP_BattleViewerHud")};
constexpr TCHAR battle_viewer_widget_package_name[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BattleViewerHud")};
constexpr TCHAR benchmark_widget_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WBP_BenchmarkHud.WBP_BenchmarkHud")};
constexpr TCHAR benchmark_widget_package_name[]{TEXT("/SpaceGame/UI/InGame/WBP_BenchmarkHud")};
FName const scripted_level_generation_context{TEXT("GenerateScriptedLevelAssets")};
auto constexpr& runtime_config_package_name{ioj::s7_level_config::canonical_package_name};
auto constexpr& runtime_config_asset_name{ioj::s7_level_config::canonical_asset_name};
constexpr TCHAR player_controller_object_path[]{
    TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController.BP_SpaceGamePlayerController")};
constexpr TCHAR player_controller_package_name[]{
    TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController")};
constexpr TCHAR player_controller_asset_name[]{TEXT("BP_SpaceGamePlayerController")};
constexpr TCHAR scripted_level_source_config_object_path[]{
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
                                                                  scripted_level_generation_context,
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
                      TCHAR const* const label) -> ioj::UMenuButtonWidget* {
    auto* const button{tree.ConstructWidget<ioj::UMenuButtonWidget>(&button_class, name)};
    button->bIsVariable = true;
    button->set_text(FText::FromString(label));
    auto* const slot{parent.AddChildToVerticalBox(button)};
    slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    return button;
}

auto generate_menu_button_widget() -> UClass* {
    auto* const blueprint{load_or_create_widget_blueprint(menu_button_object_path,
                                                          menu_button_package_name,
                                                          TEXT("WBP_MenuButton"),
                                                          *ioj::UMenuButtonWidget::StaticClass())};
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
    auto* const blueprint{load_or_create_widget_blueprint(root_layout_object_path,
                                                          root_layout_package_name,
                                                          TEXT("WBP_GameUiRoot"),
                                                          *ioj::UGameUiRootLayout::StaticClass())};
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
    auto* const blueprint{load_or_create_widget_blueprint(pause_menu_widget_object_path,
                                                          pause_menu_widget_package_name,
                                                          TEXT("WBP_PauseMenu"),
                                                          *ioj::UPauseMenuWidget::StaticClass())};
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

    heading->SetText(FText::FromString(TEXT("Options")));
    auto* const options{make_widget<ioj::UOptionsWidget>(tree, TEXT("options_widget"))};
    page->AddChildToVerticalBox(options)->SetSize(FSlateChildSize{ESlateSizeRule::Fill});

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_level_completion_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(completion_widget_object_path,
                                        completion_widget_package_name,
                                        TEXT("WBP_LevelCompletion"),
                                        *ioj::ULevelCompletionWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    tree.RootWidget = make_widget<UNativeWidgetHost>(tree, TEXT("view_host"));
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_main_menu_widget() -> UClass* {
    auto* const blueprint{load_or_create_widget_blueprint(main_menu_widget_object_path,
                                                          main_menu_widget_package_name,
                                                          TEXT("WBP_MainMenu"),
                                                          *ioj::UMainMenuWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }
    blueprint->WidgetTree->RootWidget = nullptr;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_battle_viewer_widget() -> UClass* {
    auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return nullptr;
    }

    auto const value_class{ui_data->get_widget_class<ioj::UValueWidget>()};
    if (!IsValid(value_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load Battle Viewer child widget classes"));
        return nullptr;
    }

    auto* const blueprint{
        load_or_create_widget_blueprint(battle_viewer_widget_object_path,
                                        battle_viewer_widget_package_name,
                                        TEXT("WBP_BattleViewerHud"),
                                        *ioj::UBattleViewerHudWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    auto& tree{*blueprint->WidgetTree};
    auto* const root{make_widget<UOverlay>(tree, TEXT("battle_viewer_root"))};

    auto* const force_status{
        make_widget<ioj::UForceStatusWidget>(tree, TEXT("force_status_widget"))};
    auto* const force_slot{root->AddChildToOverlay(force_status)};
    force_slot->SetHorizontalAlignment(HAlign_Left);
    force_slot->SetVerticalAlignment(VAlign_Top);
    force_slot->SetPadding(FMargin{24.0f});

    auto* const controls{make_widget<UVerticalBox>(tree, TEXT("observer_controls"))};
    auto* const controls_slot{root->AddChildToOverlay(controls)};
    controls_slot->SetHorizontalAlignment(HAlign_Center);
    controls_slot->SetVerticalAlignment(VAlign_Top);
    controls_slot->SetPadding(FMargin{24.0f});

    auto* const camera_speed{
        make_widget<ioj::UValueWidget>(tree, *value_class.Get(), TEXT("camera_speed_widget"))};
    controls->AddChildToVerticalBox(camera_speed);
    auto* const controls_label{make_widget<UTextBlock>(tree, TEXT("controls_label"))};
    controls_label->SetAutoWrapText(true);
    controls->AddChildToVerticalBox(controls_label)->SetPadding(FMargin{0.0f, 4.0f, 0.0f, 0.0f});

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto generate_benchmark_widget(UClass& button_class) -> UClass* {
    auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return nullptr;
    }

    auto const value_class{ui_data->get_widget_class<ioj::UValueWidget>()};
    if (!IsValid(value_class)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load Benchmark HUD value widget class"));
        return nullptr;
    }

    auto* const blueprint{
        load_or_create_widget_blueprint(benchmark_widget_object_path,
                                        benchmark_widget_package_name,
                                        TEXT("WBP_BenchmarkHud"),
                                        *ioj::UBenchmarkHudWidget::StaticClass())};
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
        make_widget<ioj::UValueWidget>(tree, *value_class.Get(), TEXT("ticks_remaining_widget"))};
    content->AddChildToVerticalBox(ticks)->SetPadding(FMargin{0.0f, 8.0f, 0.0f, 12.0f});
    auto* const actions{make_widget<UHorizontalBox>(tree, TEXT("run_actions"))};
    content->AddChildToVerticalBox(actions);
    auto* const pause_button{
        make_widget<ioj::UMenuButtonWidget>(tree, button_class, TEXT("pause_resume_button"))};
    pause_button->set_text(INVTEXT("Pause"));
    actions->AddChildToHorizontalBox(pause_button)->SetPadding(FMargin{0.0f, 0.0f, 8.0f, 0.0f});
    auto* const end_button{
        make_widget<ioj::UMenuButtonWidget>(tree, button_class, TEXT("end_benchmark_button"))};
    end_button->set_text(INVTEXT("End"));
    actions->AddChildToHorizontalBox(end_button);

    tree.RootWidget = root;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto configure_ui_data(UClass& root_class,
                       UClass& button_class,
                       UClass& main_class,
                       UClass& level_class,
                       UClass& pause_class,
                       UClass& completion_class,
                       UClass& battle_viewer_class,
                       UClass& benchmark_class) -> bool {
    auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
    if (!IsValid(ui_data)) {
        return false;
    }

    ui_data->Modify();
    auto const native_classes{ioj::UTestBatchGameUiData::get_native_widget_classes()};
    for (auto entry{ui_data->widget_classes.classes.CreateIterator()}; entry; ++entry) {
        if (!native_classes.Contains(entry.Key().Get())) {
            entry.RemoveCurrent();
        }
    }

    ui_data->widget_classes.classes.Add(ioj::UGameUiRootLayout::StaticClass(), &root_class);
    ui_data->widget_classes.classes.Add(ioj::UMenuButtonWidget::StaticClass(), &button_class);
    ui_data->widget_classes.classes.Add(ioj::UMainMenuWidget::StaticClass(), &main_class);
    ui_data->widget_classes.classes.Add(ioj::ULevelSelectWidget::StaticClass(), &level_class);
    ui_data->widget_classes.classes.Add(ioj::UPauseMenuWidget::StaticClass(), &pause_class);
    ui_data->widget_classes.classes.Add(ioj::ULevelCompletionWidget::StaticClass(),
                                        &completion_class);
    ui_data->widget_classes.classes.Add(ioj::UBattleViewerHudWidget::StaticClass(),
                                        &battle_viewer_class);
    ui_data->widget_classes.classes.Add(ioj::UBenchmarkHudWidget::StaticClass(), &benchmark_class);
    return save_asset(*ui_data);
}

auto generate_level_select_widget() -> UClass* {
    auto* const blueprint{
        load_or_create_widget_blueprint(widget_object_path,
                                        widget_package_name,
                                        TEXT("WBP_LevelSelect"),
                                        *ioj::s7::UScriptLevelSelectWidget::StaticClass())};
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
                                        *ioj::USaveGameViewerWidget::StaticClass())};
    if (!IsValid(blueprint)) {
        return nullptr;
    }

    blueprint->WidgetTree->RootWidget = nullptr;
    return compile_and_save(*blueprint) ? blueprint->GeneratedClass.Get() : nullptr;
}

auto controller_blueprint_is_input_free(UBlueprint& blueprint) -> bool {
    if (blueprint.ParentClass != ioj::ASpaceGamePlayerController::StaticClass()) {
        return false;
    }
    TArray<UEdGraph*> graphs;
    blueprint.GetAllGraphs(graphs);
    for (auto const* const graph : graphs) {
        for (auto const& node_ptr : graph->Nodes) {
            auto const* const node{node_ptr.Get()};
            if (graph->GetFName() == TEXT("UserConstructionScript") &&
                node->GetClass()->GetFName() == TEXT("K2Node_FunctionEntry")) {
                continue;
            }

            UE_LOG(LogTemp,
                   Error,
                   TEXT("Production controller contains graph node: %s/%s"),
                   *graph->GetName(),
                   *node->GetClass()->GetName());
            return false;
        }
    }

    if (blueprint.SimpleConstructionScript != nullptr &&
        !blueprint.SimpleConstructionScript->GetAllNodes().IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("Production controller contains Blueprint components"));
        return false;
    }

    auto const* const generated{Cast<UBlueprintGeneratedClass>(blueprint.GeneratedClass)};
    if (generated == nullptr) {
        return false;
    }

    for (auto const& binding_ptr : generated->DynamicBindingObjects) {
        auto const* const binding{binding_ptr.Get()};
        if (binding != nullptr && binding->IsA<UInputDelegateBinding>()) {
            UE_LOG(LogTemp, Error, TEXT("Production controller contains Blueprint input binding"));
            return false;
        }
    }

    return true;
}
auto load_or_create_player_controller() -> UBlueprint* {
    auto* blueprint{LoadObject<UBlueprint>(nullptr, player_controller_object_path)};
    if (!IsValid(blueprint)) {
        auto* const package{CreatePackage(player_controller_package_name)};
        blueprint =
            FKismetEditorUtilities::CreateBlueprint(ioj::ASpaceGamePlayerController::StaticClass(),
                                                    package,
                                                    FName{player_controller_asset_name},
                                                    BPTYPE_Normal,
                                                    UBlueprint::StaticClass(),
                                                    UBlueprintGeneratedClass::StaticClass());
        if (!IsValid(blueprint)) {
            UE_LOG(LogTemp, Error, TEXT("Could not create player controller Blueprint"));
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(blueprint);
    }

    if (blueprint->ParentClass != ioj::ASpaceGamePlayerController::StaticClass()) {
        UBlueprintEditorLibrary::ReparentBlueprint(blueprint,
                                                   ioj::ASpaceGamePlayerController::StaticClass());
    }
    TArray<UEdGraph*> graphs;
    blueprint->GetAllGraphs(graphs);
    for (auto* const graph : graphs) {
        TArray<UEdGraphNode*> const nodes{graph->Nodes};
        for (auto* const node : nodes) {
            if (graph->GetFName() == TEXT("UserConstructionScript") &&
                node->GetClass()->GetFName() == TEXT("K2Node_FunctionEntry")) {
                continue;
            }

            FBlueprintEditorUtils::RemoveNode(blueprint, node, true);
        }
    }

    if (auto* const construction_script{blueprint->SimpleConstructionScript.Get()}) {
        auto const components{construction_script->GetAllNodes()};
        for (auto const& component : components) {
            construction_script->RemoveNode(component);
        }
    }

    FKismetEditorUtilities::CompileBlueprint(blueprint);
    if (blueprint->Status == BS_Error || !IsValid(blueprint->GeneratedClass)) {
        UE_LOG(LogTemp, Error, TEXT("BP_SpaceGamePlayerController failed to compile"));
        return nullptr;
    }

    if (!controller_blueprint_is_input_free(*blueprint)) {
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

auto load_or_create_runtime_config(UClass& player_controller_class) -> ioj::USpaceGameLevelConfig* {
    auto const object_path{
        FString::Printf(TEXT("%s.%s"), runtime_config_package_name, runtime_config_asset_name)};
    auto* const source{
        LoadObject<ioj::USpaceGameLevelConfig>(nullptr, scripted_level_source_config_object_path)};
    if (!IsValid(source)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load source level config"));
        return nullptr;
    }

    auto* config{LoadObject<ioj::USpaceGameLevelConfig>(nullptr, *object_path)};
    if (!IsValid(config)) {
        auto* const package{CreatePackage(runtime_config_package_name)};
        config = Cast<ioj::USpaceGameLevelConfig>(StaticDuplicateObject(
            source, package, runtime_config_asset_name, RF_Public | RF_Standalone));
        if (!IsValid(config)) {
            UE_LOG(LogTemp, Error, TEXT("Could not duplicate runtime level config"));
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(config);
    }

    config->Modify();
    for (TFieldIterator<FProperty> property{ioj::USpaceGameLevelConfig::StaticClass(),
                                            EFieldIteratorFlags::ExcludeSuper};
         property;
         ++property) {
        property->CopyCompleteValue_InContainer(config, source);
    }
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

    auto* const orchestrator{world->SpawnActor<ioj::ATestBatchOrchestrator>()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogTemp, Error, TEXT("Could not spawn runtime orchestrator"));
        return false;
    }
    orchestrator->set_level_config(*config);

    auto* const starfield{world->SpawnActor<ioj::AGpuStarfieldActor>()};
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

    world_settings->DefaultGameMode = ioj::AMainMenuGameMode::StaticClass();

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
    auto const map_generated{generate_runtime_map() && generate_main_menu_map()};
    return ui_generated && map_generated ? 0 : 1;
}
