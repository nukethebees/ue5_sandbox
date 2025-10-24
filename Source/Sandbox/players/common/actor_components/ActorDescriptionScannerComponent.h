#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "ActorDescriptionScannerComponent.generated.h"

class UItemDescriptionHUDWidget;

DECLARE_DELEGATE_OneParam(FOnDescriptionUpdate, FText const&);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActorDescriptionScannerComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UActorDescriptionScannerComponent", LogSandboxActor> {
    GENERATED_BODY()
  public:
    UActorDescriptionScannerComponent();

    FOnDescriptionUpdate on_description_update;
  protected:
    virtual void BeginPlay() override;
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Description")
    float raycast_distance{2000.0f};

    void perform_raycast(FVector position, FRotator rotation);

    void set_hud_widget(UItemDescriptionHUDWidget* widget);
  private:
    TWeakObjectPtr<AActor> last_seen_actor{nullptr};
    TWeakObjectPtr<UItemDescriptionHUDWidget> hud_widget{nullptr};
};
