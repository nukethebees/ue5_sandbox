#include "Sandbox/misc/learning/actors/TextPopupTriggerActor.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBox.h"

#include "Sandbox/ui/widgets/slate/TextPopupWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATextPopupTriggerActor::ATextPopupTriggerActor()
    : collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))} {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Static);

    collision_box->SetupAttachment(RootComponent);
    collision_box->SetBoxExtent(FVector{100.0f, 100.0f, 100.0f});
    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_box->SetGenerateOverlapEvents(true);

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATextPopupTriggerActor::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(collision_box);
    collision_box->OnComponentBeginOverlap.AddDynamic(this,
                                                      &ATextPopupTriggerActor::on_overlap_begin);
    collision_box->OnComponentEndOverlap.AddDynamic(this, &ATextPopupTriggerActor::on_overlap_end);
}

void ATextPopupTriggerActor::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                              AActor* other_actor,
                                              UPrimitiveComponent* other_component,
                                              int32 other_body_index,
                                              bool from_sweep,
                                              FHitResult const& sweep_result) {
    RETURN_IF_NULLPTR(other_actor);
    RETURN_IF_NULLPTR(GEngine);
    RETURN_IF_NULLPTR(GEngine->GameViewport);

    log_verbose(TEXT("Overlap begin with actor: %s"), *other_actor->GetName());
    if (popup_widget.IsValid()) {
        log_warning(TEXT("Widget already exists, not creating a new one"));
        return;
    }

    // clang-format off
    popup_widget =
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextPopupWidget)
            .msg(popup_message)
            .OnDismissed(FOnClicked::CreateLambda([this]() {
                remove_popup_widget();
                return FReply::Handled();
            }))
        ];
    // clang-format on
    RETURN_IF_FALSE(popup_widget.IsValid());

    GEngine->GameViewport->AddViewportWidgetContent(popup_widget.ToSharedRef());
    log_verbose(TEXT("Widget added to viewport"));
}

void ATextPopupTriggerActor::on_overlap_end(UPrimitiveComponent* overlapped_component,
                                            AActor* other_actor,
                                            UPrimitiveComponent* other_component,
                                            int32 other_body_index) {
    RETURN_IF_NULLPTR(other_actor);
    log_verbose(TEXT("Overlap end with actor: %s"), *other_actor->GetName());
    remove_popup_widget();
}

void ATextPopupTriggerActor::remove_popup_widget() {
    RETURN_IF_NULLPTR(GEngine);
    RETURN_IF_NULLPTR(GEngine->GameViewport);

    if (!popup_widget.IsValid()) {
        return;
    }

    GEngine->GameViewport->RemoveViewportWidgetContent(popup_widget.ToSharedRef());
    popup_widget.Reset();
    log_verbose(TEXT("Widget removed from viewport"));
}
