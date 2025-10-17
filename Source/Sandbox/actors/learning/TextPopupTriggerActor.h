#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/slate_widgets/TextPopupWidget.h"

#include "TextPopupTriggerActor.generated.h"

UCLASS()
class SANDBOX_API ATextPopupTriggerActor
    : public AActor
    , public ml::LogMsgMixin<"ATextPopupTriggerActor"> {
    GENERATED_BODY()
  public:
    ATextPopupTriggerActor();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UFUNCTION()
    void on_overlap_end(UPrimitiveComponent* overlapped_component,
                        AActor* other_actor,
                        UPrimitiveComponent* other_component,
                        int32 other_body_index);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup")
    FText popup_message{FText::FromString(TEXT("Hello! You've entered the trigger."))};
  private:
    TSharedPtr<STextPopupWidget> popup_widget{};
};
