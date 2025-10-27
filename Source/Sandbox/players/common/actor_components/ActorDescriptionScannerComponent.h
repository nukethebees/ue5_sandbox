#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/data/ScreenBounds.h"

#include "ActorDescriptionScannerComponent.generated.h"

class AActor;
class APlayerController;

class UItemDescriptionHUDWidget;

struct FActorCorners;

DECLARE_DELEGATE_OneParam(FOnDescriptionUpdate, FText const&);
DECLARE_DELEGATE_OneParam(FOnTargetScreenBoundsUpdate, FActorCorners const&);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActorDescriptionScannerComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UActorDescriptionScannerComponent", LogSandboxActor> {
    GENERATED_BODY()
  public:
    UActorDescriptionScannerComponent();

    FOnDescriptionUpdate on_description_update;
    FOnTargetScreenBoundsUpdate on_target_screen_bounds_update;
  protected:
    virtual void BeginPlay() override;
  public:
    void perform_raycast(APlayerController const& pc, FVector position, FRotator rotation);
    void set_hud_widget(UItemDescriptionHUDWidget* widget);
    void update_outline(APlayerController const& pc, AActor const& actor);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Description")
    float raycast_distance{2000.0f};
  private:
    TWeakObjectPtr<AActor> last_seen_actor{nullptr};
    TWeakObjectPtr<UItemDescriptionHUDWidget> hud_widget{nullptr};
};
