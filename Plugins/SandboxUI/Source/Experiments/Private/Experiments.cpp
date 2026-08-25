#include "HeatmapRDG/HeatmapRDGEditorUtilityWidget.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FExperimentsModule"

class FExperimentsModule : public IModuleInterface {
  public:
    void StartupModule() override {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxUI"))};
        check(plugin.IsValid());

        auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/SandboxUI"), shader_directory);

        post_engine_init_handle_ = FCoreDelegates::GetOnPostEngineInit().AddRaw(
            this, &FExperimentsModule::register_demo_tab);
    }

    void ShutdownModule() override {
        if (post_engine_init_handle_.IsValid()) {
            FCoreDelegates::GetOnPostEngineInit().Remove(post_engine_init_handle_);
            post_engine_init_handle_.Reset();
        }
        if (demo_tab_registered_) {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(demo_tab_name);
            demo_tab_registered_ = false;
        }
        demo_widget_ = nullptr;
    }
  private:
    void register_demo_tab() {
        auto& tab_spawner{
            FGlobalTabmanager::Get()
                ->RegisterNomadTabSpawner(
                    demo_tab_name,
                    FOnSpawnTab::CreateRaw(this, &FExperimentsModule::spawn_demo_tab))
                .SetDisplayName(LOCTEXT("HeatmapRDGDemoTab", "RDG Heatmap Experiment"))
                .SetTooltipText(
                    LOCTEXT("HeatmapRDGDemoTabTooltip", "Open the C++ RDG heatmap editor utility."))
                .SetMenuType(ETabSpawnerMenuType::Enabled)};
        tab_spawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
        demo_tab_registered_ = true;
    }

    auto spawn_demo_tab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
        auto const tab{SNew(SDockTab).TabRole(ETabRole::NomadTab)};

        auto* const editor_world{GEditor->GetEditorWorldContext().World()};
        if (editor_world == nullptr) {
            tab->SetContent(SNew(STextBlock)
                                .Text(LOCTEXT("HeatmapRDGNoEditorWorld",
                                              "The editor world is not available.")));
            return tab;
        }

        auto* const widget{CreateWidget<UHeatmapRDGEditorUtilityWidget>(editor_world)};
        if (widget == nullptr) {
            tab->SetContent(SNew(STextBlock)
                                .Text(LOCTEXT("HeatmapRDGWidgetCreationFailed",
                                              "Failed to create the heatmap utility.")));
            return tab;
        }

        demo_widget_ = TStrongObjectPtr<UHeatmapRDGEditorUtilityWidget>{widget};
        tab->SetContent(widget->TakeWidget());
        tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(
            this, &FExperimentsModule::on_demo_tab_closed));
        return tab;
    }

    void on_demo_tab_closed(TSharedRef<SDockTab>) { demo_widget_ = nullptr; }

    inline static FName const demo_tab_name{TEXT("SandboxUIHeatmapRDGExperiment")};

    FDelegateHandle post_engine_init_handle_;
    TStrongObjectPtr<UHeatmapRDGEditorUtilityWidget> demo_widget_;
    bool demo_tab_registered_{false};
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FExperimentsModule, Experiments)
