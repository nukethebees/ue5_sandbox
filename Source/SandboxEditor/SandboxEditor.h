#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"

class AActor;
class FObjectPreSaveContext;
struct FPropertyChangedEvent;
class UWorld;
class FTransactionObjectEvent;

class SANDBOXEDITOR_API FSandboxEditorModule : public IModuleInterface {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void register_menu_extensions();
    void register_actor_events();
    void on_actor_added(AActor* actor);
    void on_actors_placed(UObject* asset, TArray<AActor*> const& actors);
    void on_actors_pasted(TArray<AActor*> const& actors);
    void on_map_opened(FString const& filename, bool as_template);
    void on_actor_changed(AActor* actor);
    void on_movement_finished(UObject& object);
    void on_property_changed(UObject* object, FPropertyChangedEvent& change);
    void on_transacted(UObject* object, FTransactionObjectEvent const& change);
    void on_pre_save_world(UWorld* world, FObjectPreSaveContext context);
    void play_mission(bool refresh);
    bool can_play_mission() const;
    bool poll_refresh(float delta_seconds);

    FProcHandle refresh_process_;
    FTSTicker::FDelegateHandle refresh_ticker_;
    FString refresh_mission_;
};
