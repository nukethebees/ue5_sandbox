#include "SbxMeshGenLab/SbxMeshGenLabEditorMode.h"

#include "Editor/SbxMeshGenLabEditorModeToolkit.h"
#include "Generation/MeshAssemblyRecipeAsset.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
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
    return !selected_part_indices_.IsEmpty() &&
           preview_actors_.IsValidIndex(selected_part_index_) &&
           IsValid(preview_actors_[selected_part_index_]);
}

auto USbxMeshGenLabEditorMode::ShouldDrawWidget() const -> bool {
    return UsesTransformWidget();
}

auto USbxMeshGenLabEditorMode::GetWidgetLocation() const -> FVector {
    if (!UsesTransformWidget()) {
        return FVector::ZeroVector;
    }
    if (get_settings()->selection_pivot == ESbxMeshSelectionPivot::PrimaryPart) {
        return preview_actors_[selected_part_index_]->GetActorLocation();
    }

    FVector pivot{FVector::ZeroVector};
    int32 valid_actor_count{};
    for (int32 const part_index : selected_part_indices_) {
        if (preview_actors_.IsValidIndex(part_index) && IsValid(preview_actors_[part_index])) {
            pivot += preview_actors_[part_index]->GetActorLocation();
            ++valid_actor_count;
        }
    }
    return valid_actor_count > 0 ? pivot / valid_actor_count : FVector::ZeroVector;
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

    auto const pivot{GetWidgetLocation()};
    auto const rotation_delta{rotation.Quaternion()};
    auto const primary_scale{preview_actors_[selected_part_index_]->GetActorScale3D()};
    FVector scale_factor{FVector::OneVector};
    if (!scale.IsNearlyZero()) {
        scale_factor.X = FMath::Max(primary_scale.X + scale.X, 0.001) / primary_scale.X;
        scale_factor.Y = FMath::Max(primary_scale.Y + scale.Y, 0.001) / primary_scale.Y;
        scale_factor.Z = FMath::Max(primary_scale.Z + scale.Z, 0.001) / primary_scale.Z;
    }

    for (int32 const part_index : selected_part_indices_) {
        if (!preview_actors_.IsValidIndex(part_index) || !IsValid(preview_actors_[part_index])) {
            continue;
        }

        auto* const actor{preview_actors_[part_index].Get()};
        auto transform{actor->GetActorTransform()};
        auto relative_location{transform.GetLocation() - pivot};
        relative_location *= scale_factor;
        relative_location = rotation_delta.RotateVector(relative_location);
        transform.SetLocation(pivot + relative_location + drag);
        transform.ConcatenateRotation(rotation_delta);
        transform.NormalizeRotation();

        auto new_scale{transform.GetScale3D() * scale_factor};
        new_scale.X = FMath::Max(new_scale.X, 0.001);
        new_scale.Y = FMath::Max(new_scale.Y, 0.001);
        new_scale.Z = FMath::Max(new_scale.Z, 0.001);
        transform.SetScale3D(new_scale);
        actor->SetActorTransform(transform, false, nullptr, ETeleportType::TeleportPhysics);
    }

    sync_selected_part_transforms_from_actors();
    mark_recipe_dirty();
    status_ = FText::Format(LOCTEXT("PartsMoved", "Transforming {0} selected part(s)."),
                            FText::AsNumber(selected_part_indices_.Num()));
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
            if (click.IsControlDown() || click.IsShiftDown()) {
                auto part_indices{selected_part_indices_};
                if (part_indices.Contains(part_index)) {
                    if (part_indices.Num() > 1) {
                        part_indices.Remove(part_index);
                    }
                } else {
                    part_indices.Add(part_index);
                }
                select_parts(part_indices, part_index);
            } else {
                select_part(part_index);
            }
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

    TArray<int32> part_indices;
    for (FSelectionIterator iterator{*GEditor->GetSelectedActors()}; iterator; ++iterator) {
        auto const part_index{find_preview_actor(Cast<AActor>(*iterator))};
        if (part_index != INDEX_NONE) {
            part_indices.Add(part_index);
        }
    }
    if (!part_indices.IsEmpty()) {
        select_parts(part_indices, part_indices.Last());
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

auto USbxMeshGenLabEditorMode::get_selected_part_indices() const -> TArray<int32> const& {
    return selected_part_indices_;
}

auto USbxMeshGenLabEditorMode::can_remove_selected_parts() const -> bool {
    return !selected_part_indices_.IsEmpty() && selected_part_indices_.Num() < parts_.Num();
}

auto USbxMeshGenLabEditorMode::get_status() const -> FText const& {
    return status_;
}

auto USbxMeshGenLabEditorMode::get_recipe_document_text() const -> FText {
    if (current_recipe_ == nullptr) {
        return LOCTEXT("UntitledRecipe", "Untitled assembly  (not saved)");
    }

    auto const recipe_name{FText::FromString(current_recipe_->GetPathName())};
    return recipe_dirty_
             ? FText::Format(LOCTEXT("DirtyRecipeDocument", "{0}  (unsaved changes)"), recipe_name)
             : recipe_name;
}

auto USbxMeshGenLabEditorMode::has_current_recipe() const -> bool {
    return current_recipe_ != nullptr;
}

auto USbxMeshGenLabEditorMode::on_session_changed() -> FOnSbxMeshSessionChanged& {
    return session_changed_;
}

void USbxMeshGenLabEditorMode::select_part(int32 const part_index) {
    select_parts({part_index}, part_index);
}

void USbxMeshGenLabEditorMode::select_parts(TArray<int32> const& part_indices,
                                            int32 const primary_part_index) {
    TArray<int32> valid_part_indices;
    for (int32 const part_index : part_indices) {
        if (parts_.IsValidIndex(part_index)) {
            valid_part_indices.AddUnique(part_index);
        }
    }
    if (valid_part_indices.IsEmpty()) {
        return;
    }

    selected_part_indices_ = MoveTemp(valid_part_indices);
    selected_part_index_ = selected_part_indices_.Contains(primary_part_index)
                             ? primary_part_index
                             : selected_part_indices_.Last();

    auto const part_index{selected_part_index_};
    if (!parts_.IsValidIndex(part_index)) {
        return;
    }

    auto* const settings{get_settings()};
    auto const asset_name{settings->asset_name};
    settings->load_request(parts_[part_index].mesh);
    settings->load_transform(parts_[part_index].transform);
    settings->asset_name = asset_name;
    select_preview_actors();

    status_ =
        selected_part_indices_.Num() == 1
            ? FText::Format(LOCTEXT("PartSelected", "Selected part {0}."),
                            FText::AsNumber(part_index + 1))
            : FText::Format(LOCTEXT("PartsSelected", "Selected {0} parts; part {1} is primary."),
                            FText::AsNumber(selected_part_indices_.Num()),
                            FText::AsNumber(part_index + 1));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::select_all_parts() {
    TArray<int32> part_indices;
    auto const part_count{parts_.Num()};
    part_indices.Reserve(part_count);
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        part_indices.Add(part_index);
    }
    select_parts(part_indices, selected_part_index_);
}

void USbxMeshGenLabEditorMode::selection_settings_changed() {
    status_ = get_settings()->selection_pivot == ESbxMeshSelectionPivot::SelectionCenter
                ? LOCTEXT("SelectionCenterPivot", "Using the selection center as the shared pivot.")
                : LOCTEXT("PrimaryPartPivot", "Using the primary part as the shared pivot.");
    notify_session_changed(false);
}

void USbxMeshGenLabEditorMode::add_part() {
    FSbxMeshAssemblyPart part;
    part.mesh = SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box);
    parts_.Add(part);
    create_preview_actor(parts_.Num() - 1);
    mark_recipe_dirty();
    select_part(parts_.Num() - 1);
}

void USbxMeshGenLabEditorMode::duplicate_part() {
    if (selected_part_indices_.IsEmpty()) {
        return;
    }

    apply_settings(false);
    auto const* const settings{get_settings()};
    auto const original_indices{selected_part_indices_};
    auto const pivot{GetWidgetLocation() - preview_origin_};
    auto const translation_step{settings->duplicate_translation_step};
    auto const rotation_step{settings->duplicate_rotation_step};
    auto const repeat_count{FMath::Clamp(settings->duplicate_repeat_count, 1, 64)};
    TArray<int32> duplicate_indices;
    duplicate_indices.Reserve(original_indices.Num() * repeat_count);
    int32 duplicate_primary_index{INDEX_NONE};
    for (int32 repeat_index{1}; repeat_index <= repeat_count; ++repeat_index) {
        auto const rotation_delta{FRotator{rotation_step.Pitch * repeat_index,
                                           rotation_step.Yaw * repeat_index,
                                           rotation_step.Roll * repeat_index}
                                      .Quaternion()};

        for (int32 const part_index : original_indices) {
            if (!parts_.IsValidIndex(part_index)) {
                continue;
            }

            auto part{parts_[part_index]};
            FTransform transform{FRotator{part.transform.rotation},
                                 FVector{part.transform.translation},
                                 FVector{part.transform.scale}};
            auto const relative_location{transform.GetLocation() - pivot};
            transform.SetLocation(pivot + rotation_delta.RotateVector(relative_location) +
                                  translation_step * repeat_index);
            transform.ConcatenateRotation(rotation_delta);
            transform.NormalizeRotation();
            part.transform.translation = FVector3f{transform.GetLocation()};
            part.transform.rotation = FRotator3f{transform.Rotator()};

            auto const duplicate_index{parts_.Add(part)};
            create_preview_actor(duplicate_index);
            duplicate_indices.Add(duplicate_index);
            if (part_index == selected_part_index_ && repeat_index == repeat_count) {
                duplicate_primary_index = duplicate_index;
            }
        }
    }

    mark_recipe_dirty();
    select_parts(duplicate_indices, duplicate_primary_index);
    status_ = FText::Format(LOCTEXT("PartsDuplicated", "Created {0} repeated part(s)."),
                            FText::AsNumber(duplicate_indices.Num()));
    notify_session_changed(false);
}

void USbxMeshGenLabEditorMode::remove_part() {
    if (!can_remove_selected_parts()) {
        return;
    }

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }

    auto indices_to_remove{selected_part_indices_};
    indices_to_remove.Sort([](int32 const left, int32 const right) { return left > right; });
    auto const next_selection{
        FMath::Min(indices_to_remove.Last(), parts_.Num() - indices_to_remove.Num() - 1)};
    for (int32 const part_index : indices_to_remove) {
        auto* const actor{preview_actors_[part_index].Get()};
        if (IsValid(actor) && actor->GetWorld() != nullptr) {
            actor->GetWorld()->DestroyActor(actor);
        }
        parts_.RemoveAt(part_index);
        preview_actors_.RemoveAt(part_index);
    }

    mark_recipe_dirty();
    select_part(next_selection);
}

void USbxMeshGenLabEditorMode::new_assembly() {
    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
    destroy_preview_actors();

    auto* const settings{get_settings()};
    settings->load_request(SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box));
    settings->load_transform({});
    settings->asset_name = TEXT("SM_GeneratedAssembly");
    settings->recipe_name = TEXT("SMR_NewAssembly");
    settings->recipe.Reset();
    current_recipe_ = nullptr;

    parts_.Reset();
    parts_.Add({settings->to_request(), settings->to_transform()});
    selected_part_index_ = 0;
    selected_part_indices_ = {0};
    create_preview_actor(0);
    select_preview_actors();

    status_ = LOCTEXT("NewAssemblyReady", "Started a new assembly.");
    recipe_dirty_ = false;
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::save_recipe() {
    if (current_recipe_ == nullptr) {
        status_ = LOCTEXT("NoCurrentRecipe", "Use Save As to create a recipe first.");
        notify_session_changed();
        return;
    }

    save_recipe_with_name(current_recipe_->GetFName());
}

void USbxMeshGenLabEditorMode::save_recipe_as() {
    save_recipe_with_name(get_settings()->recipe_name);
}

void USbxMeshGenLabEditorMode::save_recipe_with_name(FName const recipe_name) {
    apply_settings(false);

    auto* const settings{get_settings()};
    auto parts{parts_};
    for (auto& part : parts) {
        part.mesh.asset_name = settings->asset_name;
    }
    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
        notify_session_changed();
        return;
    }

    auto* const saved_recipe{
        SandboxMesh::write_mesh_assembly_recipe_asset(recipe_name, settings->asset_name, parts)};
    if (saved_recipe == nullptr) {
        status_ = LOCTEXT("RecipeSaveFailed", "Recipe save failed; see Output Log.");
        notify_session_changed();
        return;
    }

    settings->recipe = saved_recipe;
    settings->recipe_name = saved_recipe->GetFName();
    current_recipe_ = saved_recipe;
    recipe_dirty_ = false;
    status_ = FText::Format(LOCTEXT("RecipeSaved", "Saved recipe {0}."),
                            FText::FromString(saved_recipe->GetPathName()));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::load_recipe() {
    auto* const settings{get_settings()};
    auto* const selected_recipe{settings->recipe.LoadSynchronous()};
    if (selected_recipe == nullptr) {
        current_recipe_ = nullptr;
        recipe_dirty_ = true;
        status_ = LOCTEXT("RecipeDetached", "No recipe selected; the live assembly is unchanged.");
        notify_session_changed();
        return;
    }
    if (selected_recipe->format_version != 1) {
        status_ = FText::Format(
            LOCTEXT("RecipeVersionUnsupported", "Recipe format version {0} is not supported."),
            FText::AsNumber(selected_recipe->format_version));
        notify_session_changed();
        return;
    }

    auto parts{selected_recipe->to_assembly()};
    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
        notify_session_changed();
        return;
    }

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
    destroy_preview_actors();
    parts_ = MoveTemp(parts);
    settings->asset_name = selected_recipe->output_asset_name;
    settings->recipe_name = selected_recipe->GetFName();
    current_recipe_ = selected_recipe;

    auto const part_count{parts_.Num()};
    for (int32 part_index{0}; part_index < part_count; ++part_index) {
        create_preview_actor(part_index);
    }
    selected_part_index_ = INDEX_NONE;
    selected_part_indices_.Reset();
    select_part(0);
    status_ = FText::Format(LOCTEXT("RecipeLoaded", "Loaded recipe {0}."),
                            FText::FromString(selected_recipe->GetPathName()));
    recipe_dirty_ = false;
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::apply_settings() {
    apply_settings(true);
}

void USbxMeshGenLabEditorMode::apply_settings(bool const mark_dirty) {
    if (!parts_.IsValidIndex(selected_part_index_)) {
        return;
    }

    auto* const settings{get_settings()};
    parts_[selected_part_index_].mesh = settings->to_request();
    parts_[selected_part_index_].transform = settings->to_transform();
    refresh_preview_actor(selected_part_index_, true);
    if (mark_dirty) {
        mark_recipe_dirty();
    }

    auto const validation_error{
        SandboxMesh::validate_mesh_request(parts_[selected_part_index_].mesh)};
    status_ = validation_error.IsEmpty()
                ? FText::Format(LOCTEXT("PartUpdated", "Updated part {0}."),
                                FText::AsNumber(selected_part_index_ + 1))
                : FText::FromString(validation_error);
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::save_generated_mesh() {
    apply_settings(false);

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

    new_assembly();
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

void USbxMeshGenLabEditorMode::select_preview_actors() {
    if (GEditor == nullptr || selected_part_indices_.IsEmpty()) {
        return;
    }

    changing_selection_ = true;
    GEditor->SelectNone(false, true, false);
    for (int32 const part_index : selected_part_indices_) {
        if (!preview_actors_.IsValidIndex(part_index) || !IsValid(preview_actors_[part_index])) {
            UE_LOG(LogSbxMeshGenLabEditorMode,
                   Error,
                   TEXT("Cannot select the missing preview actor for assembly part %d."),
                   part_index + 1);
            continue;
        }
        GEditor->SelectActor(preview_actors_[part_index], true, false);
    }
    GEditor->NoteSelectionChange();
    changing_selection_ = false;
}

void USbxMeshGenLabEditorMode::sync_selected_part_transforms_from_actors() {
    for (int32 const part_index : selected_part_indices_) {
        if (!parts_.IsValidIndex(part_index) || !preview_actors_.IsValidIndex(part_index) ||
            !IsValid(preview_actors_[part_index])) {
            continue;
        }

        auto const transform{preview_actors_[part_index]->GetActorTransform()};
        auto& part_transform{parts_[part_index].transform};
        part_transform.translation = FVector3f{transform.GetLocation() - preview_origin_};
        part_transform.rotation = FRotator3f{transform.Rotator()};
        part_transform.scale = FVector3f{transform.GetScale3D()};
    }

    if (parts_.IsValidIndex(selected_part_index_)) {
        get_settings()->load_transform(parts_[selected_part_index_].transform);
    }
}

void USbxMeshGenLabEditorMode::mark_recipe_dirty() {
    recipe_dirty_ = true;
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
