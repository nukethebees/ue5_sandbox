#include "SandboxEditor/SandboxEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#include "Sandbox/combat/projectiles/data_assets/BulletDataAsset.h"
#include "Sandbox/pathfinding/PatrolPath/PatrolPath.h"
#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"
#include "SandboxEditor/checks/describable_checks.h"
#include "SandboxEditor/codegen/TypedefCodeGenerator.h"
#include "SandboxEditor/slate/PlayerSkillsPropDisplay.h"
#include "SandboxEditor/slate/StrongTypedefPreview.h"
#include "SandboxEditor/utilities/patrol_points.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

namespace ml::detail {
auto null_action{
    FUIAction(FExecuteAction::CreateLambda([]() { UE_LOG(LogTemp, Warning, TEXT("Foo")); }))};
}

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
    register_custom_properties();
    create_sandbox_editor_menus();

    // FCoreDelegates bindings
    FCoreDelegates::OnActorLabelChanged.AddStatic(APatrolWaypoint::OnActorLabelChanged);
}
void FSandboxEditorModule::ShutdownModule() {
    unregister_custom_properties();

    // FCoreDelegates bindings
    FCoreDelegates::OnActorLabelChanged.RemoveAll(APatrolWaypoint::OnActorLabelChanged);
}

void FSandboxEditorModule::create_sandbox_editor_menus() {
    auto& level_editor_module{FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor")};
    auto const toolbar_menu_extender{MakeShared<FExtender>()};

    toolbar_menu_extender->AddMenuBarExtension(
        "Help",
        EExtensionHook::After,
        nullptr,
        FMenuBarExtensionDelegate::CreateRaw(
            this, &FSandboxEditorModule::create_sandbox_editor_toolbar_menu_pulldown));
    level_editor_module.GetMenuExtensibilityManager()->AddExtender(toolbar_menu_extender);

    auto& extenders{level_editor_module.GetAllLevelViewportContextMenuExtenders()};
    extenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(
        &FSandboxEditorModule::on_extend_level_editor_menu));
}
void FSandboxEditorModule::create_sandbox_editor_toolbar_menu_pulldown(
    FMenuBarBuilder& menu_bar_builder) {
    menu_bar_builder.AddPullDownMenu(
        FText::FromString("Sandbox"),
        FText::FromString("Sandbox Utilities"),
        FNewMenuDelegate::CreateRaw(
            this, &FSandboxEditorModule::create_sandbox_editor_toolbar_menu_items));
}
void FSandboxEditorModule::create_sandbox_editor_toolbar_menu_items(FMenuBuilder& menu_builder) {
    menu_builder.AddMenuEntry(
        FText::FromName(TEXT("IDescribable Check")),
        FText::FromName(TEXT("Check classes with IDescribable can be seen with hitscan")),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateStatic(ml::check_describable_actors_are_visible_to_hitscan)));

    menu_builder.AddSubMenu(FText::FromName(TEXT("Example Submenu")),
                            FText::FromName(TEXT("Example Submenu Tooltip")),
                            FNewMenuDelegate::CreateLambda([](FMenuBuilder& submenu_builder) {
                                submenu_builder.AddMenuEntry(
                                    FText::FromName(TEXT("Example Button")),
                                    FText::FromName(TEXT("Example Button Tooltip")),
                                    FSlateIcon(),
                                    ml::detail::null_action);
                            }));
}

auto
    FSandboxEditorModule::on_extend_level_editor_menu(TSharedRef<FUICommandList> const command_list,
                                                      TArray<AActor*> selected_actors)
        -> TSharedRef<FExtender> {
    auto extender(MakeShared<FExtender>());

    bool contains_patrol_path{false};
    bool contains_patrol_waypoint{false};
    for (auto* actor : selected_actors) {
        if (Cast<APatrolWaypoint>(actor)) {
            contains_patrol_waypoint = true;
            continue;
        }
        if (Cast<APatrolPath>(actor)) {
            contains_patrol_path = true;
            continue;
        }
    }

    if (contains_patrol_path && contains_patrol_waypoint) {
        // Add the sprite actions sub-menu extender
        extender->AddMenuExtension(
            "ActorTypeTools",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateStatic(
                &FSandboxEditorModule::create_sandbox_editor_context_menu_items));
    }

    return extender;
}
void FSandboxEditorModule::create_sandbox_editor_context_menu_items(FMenuBuilder& menu_builder) {
    menu_builder.BeginSection("Patrol Path", FText::FromName(TEXT("Patrol Path")));

    menu_builder.AddMenuEntry(
        FText::FromString("Add waypoints to selected paths"),
        FText::FromString("Add the selected waypoints to the selected paths"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&ml::add_selected_patrol_points_to_selected_paths)));

    menu_builder.EndSection();
}

void FSandboxEditorModule::register_menu_extensions() {
    constexpr auto logger{NestedLogger<"register_menu_extensions">()};

    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    TRY_INIT_PTR(menu, tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "GenerateTypedefs",
        FUIAction(FExecuteAction::CreateRaw(this, &FSandboxEditorModule::on_generate_typedefs)),
        FText::FromString("Generate Typedefs"),
        FText::FromString("Generate strong typedef wrapper structs"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile")));

    logger.log_log(TEXT("Registered menu extensions"));
}
void FSandboxEditorModule::register_custom_properties() {
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
void FSandboxEditorModule::unregister_custom_properties() {
    // ToolMenus are automatically cleaned up when the module is unloaded
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor")) {
        auto& property_module{
            FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")};

        property_module.UnregisterCustomPropertyTypeLayout("Dimensions");
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
