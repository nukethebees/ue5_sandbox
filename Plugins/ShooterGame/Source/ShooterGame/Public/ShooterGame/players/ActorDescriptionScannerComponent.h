#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "SandboxGameShared/ui/ScreenBounds.h"

#include "ActorDescriptionScannerComponent.generated.h"

class AActor;
class APlayerController;

class UItemDescriptionHUDWidget;

struct FActorCorners;

DECLARE_DELEGATE_OneParam(FOnDescriptionUpdate, FText const&);
DECLARE_DELEGATE_OneParam(FOnTargetScreenBoundsUpdate, FActorCorners const&);
DECLARE_DELEGATE(FOnTargetScreenBoundsCleared);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API UActorDescriptionScannerComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UActorDescriptionScannerComponent", LogShooterGameActor> {
    GENERATED_BODY()
  public:
    UActorDescriptionScannerComponent();

    FOnDescriptionUpdate on_description_update;
    FOnTargetScreenBoundsUpdate on_target_screen_bounds_update;
    FOnTargetScreenBoundsCleared on_target_screen_bounds_cleared;
  protected:
    void BeginPlay() override;
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
