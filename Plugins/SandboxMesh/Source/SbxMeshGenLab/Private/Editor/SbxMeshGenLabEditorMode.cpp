#include "SbxMeshGenLab/SbxMeshGenLabEditorMode.h"

#include "Editor/SbxMeshGenLabEditorModeToolkit.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HitProxies.h"
#include "LevelEditorViewport.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "USbxMeshGenLabEditorMode"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshGenLabEditorMode, Log, All);

FEditorModeID const USbxMeshGenLabEditorMode::mode_id{TEXT("EM_SandboxMesh")};

USbxMeshGenLabEditorMode::USbxMeshGenLabEditorMode() {
    Info =
        FEditorModeInfo{mode_id,
                        LOCTEXT("ModeName", "Sandbox Mesh"),
                        FSlateIcon{FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh")},
                        true,
                        650};
    SettingsClass = USbxMeshGenLabSettings::StaticClass();
}

void USbxMeshGenLabEditorMode::Enter() {
    Super::Enter();
    initialize_session();
}

void USbxMeshGenLabEditorMode::Exit() {
    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }

    destroy_preview_actors();

    if (GEditor != nullptr) {
        changing_selection_ = true;
        for (auto const& actor : previous_actor_selection_) {
            if (actor.IsValid()) {
                GEditor->SelectActor(actor.Get(), true, false);
            }
        }
        GEditor->NoteSelectionChange();
        changing_selection_ = false;
    }
    previous_actor_selection_.Reset();

    Super::Exit();
}

void USbxMeshGenLabEditorMode::CreateToolkit() {
    Toolkit = MakeShared<FSbxMeshGenLabEditorModeToolkit>();
}

auto USbxMeshGenLabEditorMode::UsesTransformWidget() const -> bool {
    return preview_actors_.IsValidIndex(selected_part_index_) &&
           IsValid(preview_actors_[selected_part_index_]);
}

auto USbxMeshGenLabEditorMode::ShouldDrawWidget() const -> bool {
    return UsesTransformWidget();
}

auto USbxMeshGenLabEditorMode::GetWidgetLocation() const -> FVector {
    if (!UsesTransformWidget()) {
        return FVector::ZeroVector;
    }
    return preview_actors_[selected_part_index_]->GetActorLocation();
}

auto USbxMeshGenLabEditorMode::InputDelta(FEditorViewportClient* const viewport_client,
                                          FViewport*,
                                          FVector& drag,
                                          FRotator& rotation,
                                          FVector& scale) -> bool {
    if (!UsesTransformWidget() || viewport_client == nullptr ||
        viewport_client->GetCurrentWidgetAxis() == EAxisList::None) {
        return false;
    }

    auto* const actor{preview_actors_[selected_part_index_].Get()};
    auto transform{actor->GetActorTransform()};
    transform.AddToTranslation(drag);
    transform.ConcatenateRotation(rotation.Quaternion());
    transform.NormalizeRotation();

    auto new_scale{transform.GetScale3D() + scale};
    new_scale.X = FMath::Max(new_scale.X, 0.001);
    new_scale.Y = FMath::Max(new_scale.Y, 0.001);
    new_scale.Z = FMath::Max(new_scale.Z, 0.001);
    transform.SetScale3D(new_scale);
    actor->SetActorTransform(transform, false, nullptr, ETeleportType::TeleportPhysics);

    sync_part_transform_from_actor();
    status_ = FText::Format(LOCTEXT("PartMoved", "Editing part {0}."),
                            FText::AsNumber(selected_part_index_ + 1));
    notify_session_changed(false);
    return true;
}

auto USbxMeshGenLabEditorMode::EndTracking(FEditorViewportClient*, FViewport*) -> bool {
    notify_session_changed();
    return false;
}

auto USbxMeshGenLabEditorMode::HandleClick(FEditorViewportClient* const viewport_client,
                                           HHitProxy* const hit_proxy,
                                           FViewportClick const& click) -> bool {
    if (auto const* const actor_proxy{HitProxyCast<HActor>(hit_proxy)}) {
        auto const part_index{find_preview_actor(actor_proxy->Actor)};
        if (part_index != INDEX_NONE) {
            select_part(part_index);
            return true;
        }
    }

    return Super::HandleClick(viewport_client, hit_proxy, click);
}

auto USbxMeshGenLabEditorMode::IsSelectionAllowed(AActor* const actor, bool const selecting) const
    -> bool {
    return changing_selection_ || !selecting || find_preview_actor(actor) != INDEX_NONE;
}

void USbxMeshGenLabEditorMode::ActorSelectionChangeNotify() {
    if (changing_selection_ || GEditor == nullptr) {
        return;
    }

    for (FSelectionIterator iterator{*GEditor->GetSelectedActors()}; iterator; ++iterator) {
        auto const part_index{find_preview_actor(Cast<AActor>(*iterator))};
        if (part_index != INDEX_NONE) {
            select_part(part_index);
            return;
        }
    }
}

auto USbxMeshGenLabEditorMode::get_settings() const -> USbxMeshGenLabSettings* {
    return CastChecked<USbxMeshGenLabSettings>(SettingsObject);
}

auto USbxMeshGenLabEditorMode::get_parts() const -> TArray<FSbxMeshAssemblyPart> const& {
    return parts_;
}

auto USbxMeshGenLabEditorMode::get_selected_part_index() const -> int32 {
    return selected_part_index_;
}

auto USbxMeshGenLabEditorMode::get_status() const -> FText const& {
    return status_;
}

auto USbxMeshGenLabEditorMode::on_session_changed() -> FOnSbxMeshSessionChanged& {
    return session_changed_;
}

void USbxMeshGenLabEditorMode::select_part(int32 const part_index) {
    if (!parts_.IsValidIndex(part_index)) {
        return;
    }

    selected_part_index_ = part_index;
    auto* const settings{get_settings()};
    auto const asset_name{settings->asset_name};
    settings->load_request(parts_[part_index].mesh);
    settings->load_transform(parts_[part_index].transform);
    settings->asset_name = asset_name;
    select_preview_actor();

    status_ = FText::Format(LOCTEXT("PartSelected", "Selected part {0}."),
                            FText::AsNumber(part_index + 1));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::add_part() {
    FSbxMeshAssemblyPart part;
    part.mesh = SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box);
    parts_.Add(part);
    create_preview_actor(parts_.Num() - 1);
    select_part(parts_.Num() - 1);
}

void USbxMeshGenLabEditorMode::duplicate_part() {
    if (!parts_.IsValidIndex(selected_part_index_)) {
        return;
    }

    apply_settings();
    auto part{parts_[selected_part_index_]};
    part.transform.translation.X += 25.0f;
    parts_.Add(part);
    create_preview_actor(parts_.Num() - 1);
    select_part(parts_.Num() - 1);
}

void USbxMeshGenLabEditorMode::remove_part() {
    if (!parts_.IsValidIndex(selected_part_index_) || parts_.Num() <= 1) {
        return;
    }

    auto* const actor{preview_actors_[selected_part_index_].Get()};
    if (IsValid(actor) && actor->GetWorld() != nullptr) {
        actor->GetWorld()->DestroyActor(actor);
    }
    parts_.RemoveAt(selected_part_index_);
    preview_actors_.RemoveAt(selected_part_index_);
    selected_part_index_ = FMath::Min(selected_part_index_, parts_.Num() - 1);
    select_part(selected_part_index_);
}

void USbxMeshGenLabEditorMode::apply_settings() {
    if (!parts_.IsValidIndex(selected_part_index_)) {
        return;
    }

    auto* const settings{get_settings()};
    parts_[selected_part_index_].mesh = settings->to_request();
    parts_[selected_part_index_].transform = settings->to_transform();
    refresh_preview_actor(selected_part_index_, true);

    auto const validation_error{
        SandboxMesh::validate_mesh_request(parts_[selected_part_index_].mesh)};
    status_ = validation_error.IsEmpty()
                ? FText::Format(LOCTEXT("PartUpdated", "Updated part {0}."),
                                FText::AsNumber(selected_part_index_ + 1))
                : FText::FromString(validation_error);
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::save_generated_mesh() {
    apply_settings();

    auto parts{parts_};
    auto* const settings{get_settings()};
    for (auto& part : parts) {
        part.mesh.asset_name = settings->asset_name;
    }

    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
        notify_session_changed();
        return;
    }

    auto const mesh_data{SandboxMesh::generate_mesh_assembly(parts)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, settings->asset_name, SandboxMesh::describe_mesh_assembly(parts))};
    if (static_mesh == nullptr) {
        status_ = LOCTEXT("SaveFailed", "Mesh generation failed; see Output Log.");
        notify_session_changed();
        return;
    }

    status_ = FText::Format(LOCTEXT("SaveSucceeded", "Saved {0}."),
                            FText::FromString(static_mesh->GetPathName()));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::initialize_session() {
    previous_actor_selection_.Reset();
    if (GEditor != nullptr) {
        for (FSelectionIterator iterator{*GEditor->GetSelectedActors()}; iterator; ++iterator) {
            if (auto* const actor{Cast<AActor>(*iterator)}) {
                previous_actor_selection_.Add(actor);
            }
        }
    }

    preview_origin_ = FVector::ZeroVector;
    if (GCurrentLevelEditingViewportClient != nullptr) {
        preview_origin_ = GCurrentLevelEditingViewportClient->GetViewLocation() +
                          GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 500.0;
    }

    auto* const settings{get_settings()};
    settings->load_request(SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box));
    settings->load_transform({});
    settings->asset_name = TEXT("SM_GeneratedAssembly");

    parts_.Reset();
    parts_.Add({settings->to_request(), settings->to_transform()});
    selected_part_index_ = 0;
    create_preview_actor(0);
    select_preview_actor();

    status_ = LOCTEXT("Ready", "Sandbox Mesh mode is ready.");
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::create_preview_actor(int32 const part_index) {
    if (!parts_.IsValidIndex(part_index) || preview_actors_.Num() != part_index) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Cannot create preview actor for assembly part %d."),
               part_index + 1);
        return;
    }
    preview_actors_.Add(nullptr);

    auto* const world{GetWorld()};
    if (world == nullptr) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Cannot create a preview actor because the editor world is unavailable."));
        status_ = LOCTEXT("PreviewWorldFailed", "The editor world is unavailable.");
        return;
    }

    FActorSpawnParameters spawn_parameters;
    spawn_parameters.Name = MakeUniqueObjectName(
        world->GetCurrentLevel(), AStaticMeshActor::StaticClass(), TEXT("SbxMeshPreview"));
    spawn_parameters.ObjectFlags = RF_Transient | RF_TextExportTransient;
    spawn_parameters.OverrideLevel = world->GetCurrentLevel();
    spawn_parameters.bHideFromSceneOutliner = true;
    spawn_parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    auto* const actor{world->SpawnActor<AStaticMeshActor>(spawn_parameters)};
    if (actor == nullptr) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Failed to spawn preview actor for assembly part %d."),
               part_index + 1);
        status_ = LOCTEXT("PreviewActorFailed", "Failed to create a transient preview actor.");
        return;
    }

    actor->SetActorEnableCollision(false);
    actor->SetActorLabel(FString::Printf(TEXT("Sandbox Mesh Preview %d"), part_index + 1));
    auto* const component{actor->GetStaticMeshComponent()};
    component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    component->SetMobility(EComponentMobility::Movable);

    preview_actors_[part_index] = actor;
    refresh_preview_actor(part_index, true);
}

void USbxMeshGenLabEditorMode::destroy_preview_actors() {
    for (auto const& actor_pointer : preview_actors_) {
        auto* const actor{actor_pointer.Get()};
        if (IsValid(actor) && actor->GetWorld() != nullptr) {
            actor->GetWorld()->DestroyActor(actor);
        }
    }
    preview_actors_.Reset();
}

void USbxMeshGenLabEditorMode::refresh_preview_actor(int32 const part_index,
                                                     bool const rebuild_mesh) {
    if (!parts_.IsValidIndex(part_index) || !preview_actors_.IsValidIndex(part_index) ||
        !IsValid(preview_actors_[part_index])) {
        return;
    }

    auto* const actor{preview_actors_[part_index].Get()};
    if (rebuild_mesh) {
        auto const validation_error{SandboxMesh::validate_mesh_request(parts_[part_index].mesh)};
        if (!validation_error.IsEmpty()) {
            actor->GetStaticMeshComponent()->SetStaticMesh(nullptr);
            status_ = FText::FromString(validation_error);
            return;
        }

        auto* const static_mesh{SandboxMesh::create_transient_static_mesh(
            SandboxMesh::generate_mesh(parts_[part_index].mesh))};
        if (static_mesh == nullptr) {
            UE_LOG(LogSbxMeshGenLabEditorMode,
                   Error,
                   TEXT("Failed to build the preview mesh for assembly part %d."),
                   part_index + 1);
            status_ = LOCTEXT("PreviewMeshFailed", "Failed to build a transient preview mesh.");
            return;
        }
        actor->GetStaticMeshComponent()->SetStaticMesh(static_mesh);
    }
    actor->SetActorTransform(make_part_world_transform(parts_[part_index]),
                             false,
                             nullptr,
                             ETeleportType::TeleportPhysics);
}

void USbxMeshGenLabEditorMode::select_preview_actor() {
    if (GEditor == nullptr || !preview_actors_.IsValidIndex(selected_part_index_)) {
        return;
    }

    auto* const actor{preview_actors_[selected_part_index_].Get()};
    if (!IsValid(actor)) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Cannot select the missing preview actor for assembly part %d."),
               selected_part_index_ + 1);
        return;
    }

    changing_selection_ = true;
    GEditor->SelectNone(false, true, false);
    GEditor->SelectActor(actor, true, true);
    changing_selection_ = false;
}

void USbxMeshGenLabEditorMode::sync_part_transform_from_actor() {
    if (!parts_.IsValidIndex(selected_part_index_) ||
        !preview_actors_.IsValidIndex(selected_part_index_)) {
        return;
    }

    auto const transform{preview_actors_[selected_part_index_]->GetActorTransform()};
    auto& part_transform{parts_[selected_part_index_].transform};
    part_transform.translation = FVector3f{transform.GetLocation() - preview_origin_};
    part_transform.rotation = FRotator3f{transform.Rotator()};
    part_transform.scale = FVector3f{transform.GetScale3D()};
    get_settings()->load_transform(part_transform);
}

void USbxMeshGenLabEditorMode::notify_session_changed(bool const refresh_controls) {
    session_changed_.Broadcast(refresh_controls);
    if (GEditor != nullptr) {
        GEditor->RedrawLevelEditingViewports();
    }
}

auto USbxMeshGenLabEditorMode::find_preview_actor(AActor const* const actor) const -> int32 {
    return preview_actors_.IndexOfByPredicate(
        [actor](TObjectPtr<AStaticMeshActor> const& preview_actor) {
            return preview_actor == actor;
        });
}

auto USbxMeshGenLabEditorMode::make_part_world_transform(FSbxMeshAssemblyPart const& part) const
    -> FTransform {
    return FTransform{FRotator{part.transform.rotation},
                      preview_origin_ + FVector{part.transform.translation},
                      FVector{part.transform.scale}};
}

#undef LOCTEXT_NAMESPACE
