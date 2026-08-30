#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FSbxMeshGenLabModule"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshGenLabUi, Log, All);

namespace {
FName const mesh_gen_lab_tab_name{TEXT("SandboxMesh.GenLab")};
}

class FSbxMeshGenLabModule final : public IModuleInterface {
  public:
    using ThisClass = FSbxMeshGenLabModule;

    void StartupModule() override {
        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(
                mesh_gen_lab_tab_name,
                FOnSpawnTab::CreateRaw(this, &ThisClass::spawn_mesh_gen_lab_tab))
            .SetDisplayName(LOCTEXT("TabName", "Mesh Gen Lab"))
            .SetTooltipText(LOCTEXT("TabTooltip", "Generate procedural static mesh lab assets."))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &ThisClass::register_menus));
    }

    void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(mesh_gen_lab_tab_name);
        mesh_gen_lab_widget_.Reset();
    }
  private:
    void register_menus() {
        FToolMenuOwnerScoped const owner_scope{this};
        auto const action{
            FUIAction{FExecuteAction::CreateRaw(this, &ThisClass::open_mesh_gen_lab)}};
        auto const icon{FSlateIcon{FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh")}};

        auto* const tools_menu{UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))};
        auto& tools_section{
            tools_menu->FindOrAddSection(TEXT("SandboxMesh"), LOCTEXT("Section", "Sandbox Mesh"))};
        tools_section.AddMenuEntry(
            TEXT("OpenSbxMeshGenLab"),
            LOCTEXT("MenuEntry", "Mesh Gen Lab"),
            LOCTEXT("MenuEntryTooltip", "Open the procedural mesh generation lab."),
            icon,
            action);
    }

    void open_mesh_gen_lab() { FGlobalTabmanager::Get()->TryInvokeTab(mesh_gen_lab_tab_name); }

    auto spawn_mesh_gen_lab_tab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
        auto const tab{SNew(SDockTab).TabRole(ETabRole::NomadTab)};

        if (!mesh_gen_lab_widget_.IsValid()) {
            auto* const world{GEditor != nullptr ? GEditor->GetEditorWorldContext().World()
                                                 : nullptr};
            if (world == nullptr) {
                UE_LOG(LogSbxMeshGenLabUi,
                       Error,
                       TEXT("Cannot open Mesh Gen Lab because the editor world is unavailable."));
                return tab;
            }

            mesh_gen_lab_widget_.Reset(CreateWidget<USbxMeshGenLabWidget>(world));
            if (!mesh_gen_lab_widget_.IsValid()) {
                UE_LOG(
                    LogSbxMeshGenLabUi, Error, TEXT("Failed to create the Mesh Gen Lab widget."));
                return tab;
            }
            mesh_gen_lab_widget_->SetFlags(RF_Transient);
        }

        tab->SetContent(mesh_gen_lab_widget_->TakeWidget());
        return tab;
    }

    TStrongObjectPtr<USbxMeshGenLabWidget> mesh_gen_lab_widget_;
};

IMPLEMENT_MODULE(FSbxMeshGenLabModule, SbxMeshGenLab)

#undef LOCTEXT_NAMESPACE
