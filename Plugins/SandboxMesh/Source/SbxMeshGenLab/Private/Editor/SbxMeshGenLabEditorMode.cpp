#include "SbxMeshGenLab/SbxMeshGenLabEditorMode.h"

#include "Editor/SbxMeshGenLabEditorModeToolkit.h"
#include "Generation/MeshAssemblyRecipeAsset.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HitProxies.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "USbxMeshGenLabEditorMode"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshGenLabEditorMode, Log, All);

namespace {
constexpr double max_safe_preview_coordinate{1'000'000.0};

auto is_safe_preview_transform(FTransform const& transform) -> bool {
    auto const location{transform.GetLocation()};
    auto const scale{transform.GetScale3D()};
    auto const rotation{transform.GetRotation()};
    return !location.ContainsNaN() && !scale.ContainsNaN() && !rotation.ContainsNaN() &&
           location.GetAbsMax() <= max_safe_preview_coordinate && scale.GetMin() >= 0.001 &&
           scale.GetAbsMax() <= max_safe_preview_coordinate && rotation.IsNormalized();
}

}

void USbxMeshAssemblySessionState::PostEditUndo() {
    Super::PostEditUndo();
    undo_.Broadcast();
}

auto USbxMeshAssemblySessionState::on_undo() -> FOnSbxMeshSessionUndo& {
    return undo_;
}

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
    session_state_ = NewObject<USbxMeshAssemblySessionState>(this, NAME_None, RF_Transactional);
    Super::Enter();
    initialize_session();
}

void USbxMeshGenLabEditorMode::Exit() {
    transform_transaction_.Reset();
    if (session_state_ != nullptr) {
        session_state_->on_undo().RemoveAll(this);
    }

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }

    destroy_preview();

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
    return !selected_part_indices_.IsEmpty() && parts_.IsValidIndex(selected_part_index_);
}

auto USbxMeshGenLabEditorMode::ShouldDrawWidget() const -> bool {
    return UsesTransformWidget();
}

auto USbxMeshGenLabEditorMode::GetWidgetLocation() const -> FVector {
    if (!UsesTransformWidget()) {
        return FVector::ZeroVector;
    }
    if (selected_group_index_ != INDEX_NONE) {
        return preview_origin_ + get_group_world_transform(selected_group_index_).GetLocation();
    }
    if (get_settings()->selection_pivot == ESbxMeshSelectionPivot::PrimaryPart) {
        return make_part_world_transform(parts_[selected_part_index_]).GetLocation();
    }

    FVector pivot{FVector::ZeroVector};
    int32 valid_part_count{};
    for (int32 const part_index : selected_part_indices_) {
        if (parts_.IsValidIndex(part_index)) {
            pivot += make_part_world_transform(parts_[part_index]).GetLocation();
            ++valid_part_count;
        }
    }
    return valid_part_count > 0 ? pivot / valid_part_count : FVector::ZeroVector;
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
    auto const primary_scale{selected_group_index_ != INDEX_NONE
                                 ? get_group_world_transform(selected_group_index_).GetScale3D()
                                 : FVector{parts_[selected_part_index_].transform.scale}};
    FVector scale_factor{FVector::OneVector};
    if (!scale.IsNearlyZero()) {
        scale_factor.X = FMath::Max(primary_scale.X + scale.X, 0.001) / primary_scale.X;
        scale_factor.Y = FMath::Max(primary_scale.Y + scale.Y, 0.001) / primary_scale.Y;
        scale_factor.Z = FMath::Max(primary_scale.Z + scale.Z, 0.001) / primary_scale.Z;
    }

    if (selected_group_index_ != INDEX_NONE) {
        auto transform{get_group_world_transform(selected_group_index_)};
        transform.AddToTranslation(preview_origin_);
        transform.SetLocation(transform.GetLocation() + drag);
        transform.ConcatenateRotation(rotation_delta);
        transform.NormalizeRotation();
        auto new_scale{transform.GetScale3D() * scale_factor};
        new_scale.X = FMath::Max(new_scale.X, 0.001);
        new_scale.Y = FMath::Max(new_scale.Y, 0.001);
        new_scale.Z = FMath::Max(new_scale.Z, 0.001);
        transform.SetScale3D(new_scale);
        set_group_world_transform(selected_group_index_, transform);
        rebuild_resolved_parts();
        auto const& group{session_state_->groups[selected_group_index_]};
        get_settings()->load_transform(
            {FVector3f{group.translation}, FRotator3f{group.rotation}, FVector3f{group.scale}});
    } else {
        for (int32 const part_index : selected_part_indices_) {
            if (!parts_.IsValidIndex(part_index)) {
                continue;
            }

            auto transform{make_part_world_transform(parts_[part_index])};
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
            set_part_world_transform(part_index, transform);
            refresh_preview_instance(part_index, false);
        }
        get_settings()->load_transform(
            session_state_->parts[selected_part_index_].to_part(NAME_None).transform);
    }

    mark_recipe_dirty();
    status_ = FText::Format(LOCTEXT("PartsMoved", "Transforming {0} selected part(s)."),
                            FText::AsNumber(selected_part_indices_.Num()));
    notify_session_changed(false);
    return true;
}

auto USbxMeshGenLabEditorMode::StartTracking(FEditorViewportClient*, FViewport*) -> bool {
    if (!UsesTransformWidget() || session_state_ == nullptr) {
        return false;
    }
    transform_transaction_ = MakeUnique<FScopedTransaction>(
        LOCTEXT("TransformAssemblyNodesTransaction", "Transform Mesh Assembly Nodes"));
    session_state_->Modify();
    return true;
}

auto USbxMeshGenLabEditorMode::EndTracking(FEditorViewportClient*, FViewport*) -> bool {
    auto const handled{transform_transaction_.IsValid()};
    transform_transaction_.Reset();
    if (handled) {
        notify_session_changed();
    }
    return handled;
}

auto USbxMeshGenLabEditorMode::HandleClick(FEditorViewportClient* const viewport_client,
                                           HHitProxy* const hit_proxy,
                                           FViewportClick const& click) -> bool {
    if (auto const* const instance_proxy{HitProxyCast<HInstancedStaticMeshInstance>(hit_proxy)}) {
        auto const part_index{
            find_preview_part(instance_proxy->Component, instance_proxy->InstanceIndex)};
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

auto USbxMeshGenLabEditorMode::BoxSelect(FBox& box, bool const select) -> bool {
    TArray<int32> matching_part_indices;
    auto const strict_selection{GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection};
    auto const part_count{parts_.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const bounds{get_preview_part_bounds(part_index)};
        if (!bounds.IsValid) {
            continue;
        }
        auto const matches{strict_selection ? box.IsInsideOrOn(bounds) : box.Intersect(bounds)};
        if (matches) {
            matching_part_indices.Add(part_index);
        }
    }
    apply_marquee_selection(matching_part_indices, select);
    return true;
}

auto USbxMeshGenLabEditorMode::FrustumSelect(FConvexVolume const& frustum,
                                             FEditorViewportClient*,
                                             bool const select) -> bool {
    TArray<int32> matching_part_indices;
    auto const strict_selection{GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection};
    auto const part_count{parts_.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const bounds{get_preview_part_bounds(part_index)};
        if (!bounds.IsValid) {
            continue;
        }
        bool fully_contained{};
        auto const intersects{
            frustum.IntersectBox(bounds.GetCenter(), bounds.GetExtent(), fully_contained)};
        if (intersects && (!strict_selection || fully_contained)) {
            matching_part_indices.Add(part_index);
        }
    }
    apply_marquee_selection(matching_part_indices, select);
    return true;
}

void USbxMeshGenLabEditorMode::SelectNone() {
    if (selected_part_indices_.IsEmpty() && selected_group_index_ == INDEX_NONE) {
        return;
    }
    selected_part_indices_.Reset();
    selected_part_index_ = INDEX_NONE;
    selected_group_index_ = INDEX_NONE;
    select_preview_instances();
    status_ = LOCTEXT("SelectionCleared", "Cleared the mesh assembly selection.");
    notify_session_changed();
}

auto USbxMeshGenLabEditorMode::IsSelectionAllowed(AActor* const actor, bool const selecting) const
    -> bool {
    return changing_selection_ || !selecting || actor == preview_actor_;
}

void USbxMeshGenLabEditorMode::ActorSelectionChangeNotify() {}

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

auto USbxMeshGenLabEditorMode::get_groups() const -> TArray<FSbxMeshAssemblyRecipeGroup> const& {
    return session_state_->groups;
}

auto USbxMeshGenLabEditorMode::get_selected_group_index() const -> int32 {
    return selected_group_index_;
}

auto USbxMeshGenLabEditorMode::can_remove_selected_parts() const -> bool {
    return !selected_part_indices_.IsEmpty() && selected_part_indices_.Num() < parts_.Num();
}

auto USbxMeshGenLabEditorMode::can_create_group() const -> bool {
    return selected_group_index_ != INDEX_NONE || !selected_part_indices_.IsEmpty();
}

auto USbxMeshGenLabEditorMode::can_ungroup() const -> bool {
    return session_state_ != nullptr && session_state_->groups.IsValidIndex(selected_group_index_);
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
    selected_group_index_ = INDEX_NONE;
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
    settings->load_transform(session_state_->parts[part_index].to_part(NAME_None).transform);
    settings->asset_name = asset_name;
    select_preview_instances();

    status_ =
        selected_part_indices_.Num() == 1
            ? FText::Format(LOCTEXT("PartSelected", "Selected part {0}."),
                            FText::AsNumber(part_index + 1))
            : FText::Format(LOCTEXT("PartsSelected", "Selected {0} parts; part {1} is primary."),
                            FText::AsNumber(selected_part_indices_.Num()),
                            FText::AsNumber(part_index + 1));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::select_group(int32 const group_index) {
    if (session_state_ == nullptr || !session_state_->groups.IsValidIndex(group_index)) {
        return;
    }

    auto const part_indices{get_descendant_part_indices(session_state_->groups[group_index].id)};
    if (part_indices.IsEmpty()) {
        return;
    }

    selected_group_index_ = group_index;
    selected_part_indices_ = part_indices;
    selected_part_index_ = part_indices[0];
    get_settings()->load_transform({FVector3f{session_state_->groups[group_index].translation},
                                    FRotator3f{session_state_->groups[group_index].rotation},
                                    FVector3f{session_state_->groups[group_index].scale}});
    select_preview_instances();
    status_ = FText::Format(LOCTEXT("GroupSelected", "Selected group '{0}' ({1} parts)."),
                            FText::FromName(session_state_->groups[group_index].name),
                            FText::AsNumber(part_indices.Num()));
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
    FScopedTransaction const transaction{LOCTEXT("AddAssemblyPartTransaction", "Add Mesh Part")};
    session_state_->Modify();

    FSbxMeshAssemblyPart part;
    part.mesh = SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box);
    auto const part_id{FGuid::NewGuid()};
    session_state_->parts.Add(FSbxMeshAssemblyRecipePart::from_part(part, part_id));
    parts_.Add(part);
    part_ids_.Add(part_id);
    rebuild_part_index_map();
    add_preview_instance(parts_.Num() - 1);
    mark_recipe_dirty();
    select_part(parts_.Num() - 1);
}

void USbxMeshGenLabEditorMode::duplicate_part() {
    if (selected_part_indices_.IsEmpty()) {
        return;
    }

    if (selected_group_index_ != INDEX_NONE) {
        duplicate_selected_group();
        return;
    }

    apply_settings(false);
    FScopedTransaction const transaction{
        LOCTEXT("DuplicateAssemblyPartsTransaction", "Duplicate Mesh Parts")};
    session_state_->Modify();
    auto const* const settings{get_settings()};
    auto const original_indices{selected_part_indices_};
    auto const pivot{GetWidgetLocation() - preview_origin_};
    auto const translation_step{settings->duplicate_translation_step};
    auto const rotation_step{settings->duplicate_rotation_step};
    auto const repeat_count{FMath::Clamp(settings->duplicate_repeat_count, 1, 64)};
    TArray<FSbxMeshAssemblyRecipePart> duplicate_parts;
    duplicate_parts.Reserve(original_indices.Num() * repeat_count);
    int32 duplicate_primary_offset{INDEX_NONE};
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
            FTransform const world_transform{transform.GetRotation(),
                                             preview_origin_ + transform.GetLocation(),
                                             transform.GetScale3D()};
            if (!is_safe_preview_transform(world_transform)) {
                UE_LOG(LogSbxMeshGenLabEditorMode,
                       Error,
                       TEXT("Refused duplicate %d of part %d with unsafe transform: %s"),
                       repeat_index,
                       part_index + 1,
                       *world_transform.ToHumanReadableString());
                status_ = LOCTEXT(
                    "UnsafeDuplicateTransform",
                    "Duplicate / Repeat would create an invalid or excessively distant preview "
                    "transform. Reduce the translation, rotation, or repeat count.");
                notify_session_changed(false);
                return;
            }

            auto const parent_id{session_state_->parts[part_index].parent_id};
            auto const local_transform{
                transform.GetRelativeTransform(get_parent_world_transform(parent_id))};
            part.transform = {FVector3f{local_transform.GetLocation()},
                              FRotator3f{local_transform.Rotator()},
                              FVector3f{local_transform.GetScale3D()}};
            if (part_index == selected_part_index_ && repeat_index == repeat_count) {
                duplicate_primary_offset = duplicate_parts.Num();
            }
            duplicate_parts.Add(
                FSbxMeshAssemblyRecipePart::from_part(part, FGuid::NewGuid(), parent_id));
        }
    }

    TArray<int32> duplicate_indices;
    duplicate_indices.Reserve(duplicate_parts.Num());
    int32 duplicate_primary_index{INDEX_NONE};
    auto const duplicate_count{duplicate_parts.Num()};
    for (int32 duplicate_offset{}; duplicate_offset < duplicate_count; ++duplicate_offset) {
        auto const duplicate_index{session_state_->parts.Add(duplicate_parts[duplicate_offset])};
        part_ids_.Add(duplicate_parts[duplicate_offset].id);
        duplicate_indices.Add(duplicate_index);
        if (duplicate_offset == duplicate_primary_offset) {
            duplicate_primary_index = duplicate_index;
        }
    }

    rebuild_resolved_parts();
    rebuild_part_index_map();
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

    FScopedTransaction const transaction{
        LOCTEXT("RemoveAssemblyNodesTransaction", "Remove Mesh Assembly Nodes")};
    session_state_->Modify();

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }

    TArray<FGuid> ids_to_remove;
    ids_to_remove.Reserve(selected_part_indices_.Num());
    for (int32 const part_index : selected_part_indices_) {
        ids_to_remove.Add(part_ids_[part_index]);
    }
    for (FGuid const part_id : ids_to_remove) {
        remove_preview_instance(part_id);
    }

    auto indices_to_remove{selected_part_indices_};
    indices_to_remove.Sort([](int32 const left, int32 const right) { return left > right; });
    auto const next_selection{
        FMath::Min(indices_to_remove.Last(), parts_.Num() - indices_to_remove.Num() - 1)};
    for (int32 const part_index : indices_to_remove) {
        parts_.RemoveAt(part_index);
        part_ids_.RemoveAt(part_index);
        session_state_->parts.RemoveAt(part_index);
    }
    if (selected_group_index_ != INDEX_NONE) {
        auto group_indices{
            get_descendant_group_indices(session_state_->groups[selected_group_index_].id)};
        group_indices.Sort([](int32 const left, int32 const right) { return left > right; });
        for (int32 const group_index : group_indices) {
            session_state_->groups.RemoveAt(group_index);
        }
    }

    for (int32 group_index{session_state_->groups.Num() - 1}; group_index >= 0; --group_index) {
        if (get_descendant_part_indices(session_state_->groups[group_index].id).IsEmpty()) {
            session_state_->groups.RemoveAt(group_index);
        }
    }
    rebuild_part_index_map();

    selected_group_index_ = INDEX_NONE;
    mark_recipe_dirty();
    select_part(next_selection);
}

void USbxMeshGenLabEditorMode::create_group() {
    if (!can_create_group()) {
        return;
    }

    FScopedTransaction const transaction{
        LOCTEXT("CreateAssemblyGroupTransaction", "Create Mesh Group")};
    session_state_->Modify();

    FSbxMeshAssemblyRecipeGroup group;
    group.id = FGuid::NewGuid();
    group.name = FName{FString::Printf(TEXT("Group %d"), session_state_->groups.Num() + 1)};

    if (selected_group_index_ != INDEX_NONE) {
        auto& child_group{session_state_->groups[selected_group_index_]};
        auto const child_world{get_group_world_transform(selected_group_index_)};
        group.parent_id = child_group.parent_id;
        group.set_transform(
            child_world.GetRelativeTransform(get_parent_world_transform(group.parent_id)));
        child_group.parent_id = group.id;
        child_group.set_transform(FTransform::Identity);
    } else {
        auto parent_id{session_state_->parts[selected_part_indices_[0]].parent_id};
        for (int32 const part_index : selected_part_indices_) {
            if (session_state_->parts[part_index].parent_id != parent_id) {
                parent_id.Invalidate();
                break;
            }
        }
        group.parent_id = parent_id;

        FVector pivot{FVector::ZeroVector};
        for (int32 const part_index : selected_part_indices_) {
            pivot += FVector{parts_[part_index].transform.translation};
        }
        pivot /= selected_part_indices_.Num();
        FTransform const group_world{FQuat::Identity, pivot, FVector::OneVector};
        group.set_transform(
            group_world.GetRelativeTransform(get_parent_world_transform(group.parent_id)));

        for (int32 const part_index : selected_part_indices_) {
            auto& recipe_part{session_state_->parts[part_index]};
            FTransform const part_world{FRotator{parts_[part_index].transform.rotation},
                                        FVector{parts_[part_index].transform.translation},
                                        FVector{parts_[part_index].transform.scale}};
            recipe_part.parent_id = group.id;
            auto const local_transform{part_world.GetRelativeTransform(group_world)};
            recipe_part.translation = local_transform.GetLocation();
            recipe_part.rotation = local_transform.Rotator();
            recipe_part.scale = local_transform.GetScale3D();
        }
    }

    auto const group_index{session_state_->groups.Add(MoveTemp(group))};
    rebuild_resolved_parts();
    mark_recipe_dirty();
    select_group(group_index);
}

void USbxMeshGenLabEditorMode::ungroup() {
    if (!can_ungroup()) {
        return;
    }

    FScopedTransaction const transaction{LOCTEXT("UngroupAssemblyTransaction", "Ungroup Mesh")};
    session_state_->Modify();

    auto const group_index{selected_group_index_};
    auto const group_id{session_state_->groups[group_index].id};
    auto const parent_id{session_state_->groups[group_index].parent_id};
    auto const parent_world{get_parent_world_transform(parent_id)};
    auto const selected_parts{get_descendant_part_indices(group_id)};

    auto const part_count{parts_.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto& recipe_part{session_state_->parts[part_index]};
        if (recipe_part.parent_id != group_id) {
            continue;
        }
        FTransform const part_world{FRotator{parts_[part_index].transform.rotation},
                                    FVector{parts_[part_index].transform.translation},
                                    FVector{parts_[part_index].transform.scale}};
        auto const local_transform{part_world.GetRelativeTransform(parent_world)};
        recipe_part.parent_id = parent_id;
        recipe_part.translation = local_transform.GetLocation();
        recipe_part.rotation = local_transform.Rotator();
        recipe_part.scale = local_transform.GetScale3D();
    }

    auto const group_count{session_state_->groups.Num()};
    for (int32 child_index{}; child_index < group_count; ++child_index) {
        auto& child{session_state_->groups[child_index]};
        if (child.parent_id != group_id) {
            continue;
        }
        auto const child_world{get_group_world_transform(child_index)};
        child.parent_id = parent_id;
        child.set_transform(child_world.GetRelativeTransform(parent_world));
    }
    session_state_->groups.RemoveAt(group_index);

    selected_group_index_ = INDEX_NONE;
    rebuild_resolved_parts();
    mark_recipe_dirty();
    select_parts(selected_parts, selected_parts[0]);
}

void USbxMeshGenLabEditorMode::new_assembly() {
    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
    destroy_preview();

    auto* const settings{get_settings()};
    settings->load_request(SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box));
    settings->load_transform({});
    settings->asset_name = TEXT("SM_GeneratedAssembly");
    settings->recipe_name = TEXT("SMR_NewAssembly");
    settings->recipe.Reset();
    current_recipe_ = nullptr;

    if (session_state_ != nullptr) {
        session_state_->on_undo().RemoveAll(this);
    }
    session_state_ = NewObject<USbxMeshAssemblySessionState>(this, NAME_None, RF_Transactional);
    session_state_->on_undo().AddUObject(this,
                                         &USbxMeshGenLabEditorMode::restore_session_after_undo);
    auto const part{FSbxMeshAssemblyPart{settings->to_request(), settings->to_transform()}};
    session_state_->parts = {FSbxMeshAssemblyRecipePart::from_part(part)};
    session_state_->groups.Reset();
    parts_ = {part};
    part_ids_ = {session_state_->parts[0].id};
    rebuild_part_index_map();
    selected_part_index_ = 0;
    selected_part_indices_ = {0};
    selected_group_index_ = INDEX_NONE;
    add_preview_instance(0);
    select_preview_instances();

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

    auto const hierarchy_error{SandboxMesh::validate_mesh_assembly_hierarchy(
        session_state_->parts, session_state_->groups)};
    if (!hierarchy_error.IsEmpty()) {
        status_ = FText::FromString(hierarchy_error);
        notify_session_changed();
        return;
    }

    auto* const saved_recipe{SandboxMesh::write_mesh_assembly_recipe_asset(
        recipe_name, settings->asset_name, session_state_->parts, session_state_->groups)};
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
    if (selected_recipe->format_version != 1 && selected_recipe->format_version != 2) {
        status_ = FText::Format(
            LOCTEXT("RecipeVersionUnsupported", "Recipe format version {0} is not supported."),
            FText::AsNumber(selected_recipe->format_version));
        notify_session_changed();
        return;
    }

    auto recipe_parts{selected_recipe->parts};
    auto recipe_groups{selected_recipe->groups};
    if (SandboxMesh::is_legacy_mesh_assembly_recipe(
            recipe_parts, recipe_groups, selected_recipe->format_version)) {
        recipe_groups.Reset();
        for (auto& part : recipe_parts) {
            part.id = FGuid::NewGuid();
            part.parent_id.Invalidate();
        }
    }
    auto const hierarchy_error{
        SandboxMesh::validate_mesh_assembly_hierarchy(recipe_parts, recipe_groups)};
    if (!hierarchy_error.IsEmpty()) {
        status_ = FText::FromString(hierarchy_error);
        notify_session_changed();
        return;
    }
    auto parts{SandboxMesh::resolve_mesh_assembly_hierarchy(
        recipe_parts, recipe_groups, selected_recipe->output_asset_name)};
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
    destroy_preview();
    session_state_->on_undo().RemoveAll(this);
    session_state_ = NewObject<USbxMeshAssemblySessionState>(this, NAME_None, RF_Transactional);
    session_state_->on_undo().AddUObject(this,
                                         &USbxMeshGenLabEditorMode::restore_session_after_undo);
    session_state_->parts = MoveTemp(recipe_parts);
    session_state_->groups = MoveTemp(recipe_groups);
    parts_ = MoveTemp(parts);
    part_ids_.Reset();
    part_ids_.Reserve(parts_.Num());
    for (auto const& part : session_state_->parts) {
        part_ids_.Add(part.id);
    }
    rebuild_part_index_map();
    settings->asset_name = selected_recipe->output_asset_name;
    settings->recipe_name = selected_recipe->GetFName();
    current_recipe_ = selected_recipe;

    auto const part_count{parts_.Num()};
    for (int32 part_index{0}; part_index < part_count; ++part_index) {
        add_preview_instance(part_index);
    }
    selected_part_index_ = INDEX_NONE;
    selected_part_indices_.Reset();
    selected_group_index_ = INDEX_NONE;
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
    if (settings->part_scale.GetMin() < 0.001) {
        status_ = LOCTEXT("InvalidNodeScale", "All transform scale values must be at least 0.001.");
        notify_session_changed();
        return;
    }
    TUniquePtr<FScopedTransaction> transaction;
    if (mark_dirty) {
        transaction = MakeUnique<FScopedTransaction>(
            selected_group_index_ != INDEX_NONE
                ? LOCTEXT("EditAssemblyGroupTransaction", "Edit Mesh Group")
                : LOCTEXT("EditAssemblyPartTransaction", "Edit Mesh Part"));
        session_state_->Modify();
    }
    if (selected_group_index_ != INDEX_NONE) {
        auto& group{session_state_->groups[selected_group_index_]};
        group.translation = settings->part_translation;
        group.rotation = settings->part_rotation;
        group.scale = settings->part_scale;
        rebuild_resolved_parts();
    } else {
        auto& recipe_part{session_state_->parts[selected_part_index_]};
        auto const part_id{recipe_part.id};
        auto const parent_id{recipe_part.parent_id};
        recipe_part = FSbxMeshAssemblyRecipePart::from_part(
            {settings->to_request(), settings->to_transform()}, part_id, parent_id);
        rebuild_resolved_parts();
        refresh_preview_instance(selected_part_index_, true);
    }
    if (mark_dirty) {
        mark_recipe_dirty();
    }

    auto const validation_error{
        selected_group_index_ == INDEX_NONE
            ? SandboxMesh::validate_mesh_request(parts_[selected_part_index_].mesh)
            : SandboxMesh::validate_mesh_assembly_hierarchy(session_state_->parts,
                                                            session_state_->groups)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
    } else if (selected_group_index_ != INDEX_NONE) {
        status_ =
            FText::Format(LOCTEXT("GroupUpdated", "Updated group '{0}'."),
                          FText::FromName(session_state_->groups[selected_group_index_].name));
    } else {
        status_ = FText::Format(LOCTEXT("PartUpdated", "Updated part {0}."),
                                FText::AsNumber(selected_part_index_ + 1));
    }
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

auto USbxMeshGenLabEditorMode::ensure_preview_actor() -> bool {
    if (IsValid(preview_actor_)) {
        return true;
    }
    auto* const world{GetWorld()};
    if (world == nullptr) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Cannot create the ISMC preview because the editor world is unavailable."));
        status_ = LOCTEXT("PreviewWorldFailed", "The editor world is unavailable.");
        return false;
    }

    FActorSpawnParameters spawn_parameters;
    spawn_parameters.Name = MakeUniqueObjectName(
        world->GetCurrentLevel(), AActor::StaticClass(), TEXT("SbxMeshPreview"));
    spawn_parameters.ObjectFlags = RF_Transient | RF_TextExportTransient;
    spawn_parameters.OverrideLevel = world->GetCurrentLevel();
    spawn_parameters.bHideFromSceneOutliner = true;
    spawn_parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    preview_actor_ = world->SpawnActor<AActor>(spawn_parameters);
    if (!IsValid(preview_actor_)) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Failed to spawn the transient ISMC preview actor."));
        status_ = LOCTEXT("PreviewActorFailed", "Failed to create a transient preview actor.");
        return false;
    }

    preview_actor_->SetActorEnableCollision(false);
    preview_actor_->SetActorLabel(TEXT("Sandbox Mesh Preview"));
    auto* const root{NewObject<USceneComponent>(preview_actor_, TEXT("PreviewRoot"))};
    preview_actor_->SetRootComponent(root);
    preview_actor_->AddInstanceComponent(root);
    root->RegisterComponent();
    return true;
}

auto USbxMeshGenLabEditorMode::find_or_create_preview_bucket(
    FSbxMeshGenerationRequest const& request) -> int32 {
    auto const mesh_key{SandboxMesh::describe_mesh_request(request)};
    auto const existing_index{preview_buckets_.IndexOfByPredicate(
        [&mesh_key](FSbxMeshPreviewBucket const& bucket) { return bucket.mesh_key == mesh_key; })};
    if (existing_index != INDEX_NONE) {
        return existing_index;
    }
    if (!ensure_preview_actor()) {
        return INDEX_NONE;
    }

    auto* const static_mesh{
        SandboxMesh::create_transient_static_mesh(SandboxMesh::generate_mesh(request))};
    if (static_mesh == nullptr) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Failed to build an ISMC preview mesh for %s."),
               *mesh_key);
        status_ = LOCTEXT("PreviewMeshFailed", "Failed to build a transient preview mesh.");
        return INDEX_NONE;
    }

    auto const component_name{MakeUniqueObjectName(
        preview_actor_, UInstancedStaticMeshComponent::StaticClass(), TEXT("SbxMeshInstances"))};
    auto* const component{
        NewObject<UInstancedStaticMeshComponent>(preview_actor_, component_name, RF_Transient)};
    preview_actor_->AddInstanceComponent(component);
    component->SetupAttachment(preview_actor_->GetRootComponent());
    component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    component->SetMobility(EComponentMobility::Movable);
    component->SetCanEverAffectNavigation(false);
    component->SetRemoveSwap();
    component->bHasPerInstanceHitProxies = true;
    component->SetStaticMesh(static_mesh);
    component->RegisterComponent();
    check(component->SupportsRemoveSwap());

    return preview_buckets_.Add({mesh_key, component, {}});
}

void USbxMeshGenLabEditorMode::add_preview_instance(int32 const part_index) {
    if (!parts_.IsValidIndex(part_index) || !part_ids_.IsValidIndex(part_index)) {
        return;
    }

    auto const validation_error{SandboxMesh::validate_mesh_request(parts_[part_index].mesh)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
        return;
    }

    auto const bucket_index{find_or_create_preview_bucket(parts_[part_index].mesh)};
    if (!preview_buckets_.IsValidIndex(bucket_index)) {
        return;
    }

    auto& bucket{preview_buckets_[bucket_index]};
    auto* const component{bucket.component.Get()};
    if (!IsValid(component)) {
        return;
    }

    auto const instance_index{
        component->AddInstance(make_part_world_transform(parts_[part_index]), true)};
    auto const part_id{part_ids_[part_index]};
    check(instance_index == bucket.instance_part_ids.Num());
    bucket.instance_part_ids.Add(part_id);
    preview_location_by_part_id_.Add(part_id, {bucket_index, instance_index});
}

void USbxMeshGenLabEditorMode::remove_preview_instance(FGuid const part_id) {
    auto const* const location{preview_location_by_part_id_.Find(part_id)};
    if (location == nullptr || !preview_buckets_.IsValidIndex(location->bucket_index)) {
        return;
    }

    auto& bucket{preview_buckets_[location->bucket_index]};
    auto* const component{bucket.component.Get()};
    if (!IsValid(component) || !bucket.instance_part_ids.IsValidIndex(location->instance_index)) {
        preview_location_by_part_id_.Remove(part_id);
        return;
    }

    auto const removed_index{location->instance_index};
    auto const final_index{bucket.instance_part_ids.Num() - 1};
    auto const moved_part_id{bucket.instance_part_ids[final_index]};
    if (!component->RemoveInstance(removed_index)) {
        UE_LOG(LogSbxMeshGenLabEditorMode,
               Error,
               TEXT("Failed to remove ISMC preview instance %d."),
               removed_index);
        return;
    }

    bucket.instance_part_ids.RemoveAtSwap(removed_index, EAllowShrinking::No);
    preview_location_by_part_id_.Remove(part_id);
    if (removed_index != final_index) {
        preview_location_by_part_id_.FindChecked(moved_part_id).instance_index = removed_index;
    }
}

void USbxMeshGenLabEditorMode::destroy_preview() {
    if (IsValid(preview_actor_) && preview_actor_->GetWorld() != nullptr) {
        preview_actor_->GetWorld()->DestroyActor(preview_actor_);
    }
    preview_actor_ = nullptr;
    preview_buckets_.Reset();
    preview_location_by_part_id_.Reset();
}

void USbxMeshGenLabEditorMode::refresh_preview_instance(int32 const part_index,
                                                        bool const rebuild_mesh) {
    if (!parts_.IsValidIndex(part_index) || !part_ids_.IsValidIndex(part_index)) {
        return;
    }

    auto const part_id{part_ids_[part_index]};
    if (rebuild_mesh) {
        remove_preview_instance(part_id);
        add_preview_instance(part_index);
        select_preview_instances();
        return;
    }

    auto const* const location{preview_location_by_part_id_.Find(part_id)};
    if (location == nullptr || !preview_buckets_.IsValidIndex(location->bucket_index)) {
        add_preview_instance(part_index);
        return;
    }
    auto* const component{preview_buckets_[location->bucket_index].component.Get()};
    if (IsValid(component)) {
        component->UpdateInstanceTransform(location->instance_index,
                                           make_part_world_transform(parts_[part_index]),
                                           true,
                                           true,
                                           true);
    }
}

void USbxMeshGenLabEditorMode::select_preview_instances() {
    for (auto& bucket : preview_buckets_) {
        if (auto* const component{bucket.component.Get()}) {
            component->ClearInstanceSelection();
        }
    }

    for (int32 const part_index : selected_part_indices_) {
        if (!part_ids_.IsValidIndex(part_index)) {
            continue;
        }
        auto const* const location{preview_location_by_part_id_.Find(part_ids_[part_index])};
        if (location == nullptr || !preview_buckets_.IsValidIndex(location->bucket_index)) {
            continue;
        }
        if (auto* const component{preview_buckets_[location->bucket_index].component.Get()}) {
            component->SelectInstance(true, location->instance_index);
        }
    }

    for (auto& bucket : preview_buckets_) {
        if (auto* const component{bucket.component.Get()}) {
            component->MarkRenderStateDirty();
        }
    }

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
}

void USbxMeshGenLabEditorMode::rebuild_part_index_map() {
    part_index_by_id_.Reset();
    auto const part_count{part_ids_.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        part_index_by_id_.Add(part_ids_[part_index], part_index);
    }
}

void USbxMeshGenLabEditorMode::rebuild_resolved_parts(bool const rebuild_geometry) {
    auto const validation_error{SandboxMesh::validate_mesh_assembly_hierarchy(
        session_state_->parts, session_state_->groups)};
    if (!validation_error.IsEmpty()) {
        UE_LOG(LogSbxMeshGenLabEditorMode, Error, TEXT("%s"), *validation_error);
        status_ = FText::FromString(validation_error);
        return;
    }

    parts_ = SandboxMesh::resolve_mesh_assembly_hierarchy(
        session_state_->parts, session_state_->groups, get_settings()->asset_name);
    part_ids_.Reset();
    part_ids_.Reserve(session_state_->parts.Num());
    for (auto const& part : session_state_->parts) {
        part_ids_.Add(part.id);
    }
    rebuild_part_index_map();

    if (rebuild_geometry) {
        destroy_preview();
        auto const part_count{parts_.Num()};
        for (int32 part_index{}; part_index < part_count; ++part_index) {
            add_preview_instance(part_index);
        }
        return;
    }

    auto const part_count{parts_.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        refresh_preview_instance(part_index, false);
    }
}

void USbxMeshGenLabEditorMode::duplicate_selected_group() {
    if (!session_state_->groups.IsValidIndex(selected_group_index_)) {
        return;
    }

    auto const* const settings{get_settings()};
    auto const source_group_index{selected_group_index_};
    auto const source_group_id{session_state_->groups[source_group_index].id};
    auto const source_group_indices{get_descendant_group_indices(source_group_id)};
    auto const source_part_indices{get_descendant_part_indices(source_group_id)};
    auto const source_world{get_group_world_transform(source_group_index)};
    auto const repeat_count{FMath::Clamp(settings->duplicate_repeat_count, 1, 64)};

    FScopedTransaction const transaction{
        LOCTEXT("DuplicateAssemblyGroupTransaction", "Duplicate Mesh Group")};
    session_state_->Modify();

    int32 final_root_index{INDEX_NONE};
    for (int32 repeat_index{1}; repeat_index <= repeat_count; ++repeat_index) {
        TMap<FGuid, FGuid> duplicated_ids;
        for (int32 const group_index : source_group_indices) {
            duplicated_ids.Add(session_state_->groups[group_index].id, FGuid::NewGuid());
        }

        auto const rotation_delta{FRotator{settings->duplicate_rotation_step.Pitch * repeat_index,
                                           settings->duplicate_rotation_step.Yaw * repeat_index,
                                           settings->duplicate_rotation_step.Roll * repeat_index}
                                      .Quaternion()};
        auto duplicate_root_world{source_world};
        duplicate_root_world.SetLocation(source_world.GetLocation() +
                                         settings->duplicate_translation_step * repeat_index);
        duplicate_root_world.ConcatenateRotation(rotation_delta);
        duplicate_root_world.NormalizeRotation();
        FTransform const preview_transform{duplicate_root_world.GetRotation(),
                                           preview_origin_ + duplicate_root_world.GetLocation(),
                                           duplicate_root_world.GetScale3D()};
        if (!is_safe_preview_transform(preview_transform)) {
            status_ = LOCTEXT("UnsafeDuplicateGroupTransform",
                              "Duplicate / Repeat would create an invalid group transform.");
            return;
        }

        for (int32 const group_index : source_group_indices) {
            auto group{session_state_->groups[group_index]};
            auto const source_id{group.id};
            group.id = duplicated_ids.FindChecked(source_id);
            if (source_id == source_group_id) {
                group.set_transform(duplicate_root_world.GetRelativeTransform(
                    get_parent_world_transform(group.parent_id)));
            } else {
                group.parent_id = duplicated_ids.FindChecked(group.parent_id);
            }
            auto const new_group_index{session_state_->groups.Add(MoveTemp(group))};
            if (source_id == source_group_id) {
                final_root_index = new_group_index;
            }
        }

        for (int32 const part_index : source_part_indices) {
            auto part{session_state_->parts[part_index]};
            part.id = FGuid::NewGuid();
            part.parent_id = duplicated_ids.FindChecked(part.parent_id);
            session_state_->parts.Add(MoveTemp(part));
        }
    }

    rebuild_resolved_parts();
    mark_recipe_dirty();
    select_group(final_root_index);
    status_ = FText::Format(LOCTEXT("GroupsDuplicated", "Created {0} repeated group(s)."),
                            FText::AsNumber(repeat_count));
    notify_session_changed(false);
}

void USbxMeshGenLabEditorMode::restore_session_after_undo() {
    rebuild_resolved_parts(true);
    selected_group_index_ = INDEX_NONE;
    selected_part_index_ = INDEX_NONE;
    selected_part_indices_.Reset();
    if (!parts_.IsEmpty()) {
        select_part(0);
    }
    mark_recipe_dirty();
    status_ = LOCTEXT("HierarchyUndoRestored", "Restored the mesh assembly hierarchy.");
    notify_session_changed();
}

auto USbxMeshGenLabEditorMode::get_group_world_transform(int32 const group_index) const
    -> FTransform {
    check(session_state_->groups.IsValidIndex(group_index));
    auto transform{session_state_->groups[group_index].to_transform()};
    auto parent_id{session_state_->groups[group_index].parent_id};
    while (parent_id.IsValid()) {
        auto const parent_index{session_state_->groups.IndexOfByPredicate(
            [parent_id](FSbxMeshAssemblyRecipeGroup const& group) {
                return group.id == parent_id;
            })};
        check(session_state_->groups.IsValidIndex(parent_index));
        transform *= session_state_->groups[parent_index].to_transform();
        parent_id = session_state_->groups[parent_index].parent_id;
    }
    return transform;
}

auto USbxMeshGenLabEditorMode::get_parent_world_transform(FGuid const parent_id) const
    -> FTransform {
    if (!parent_id.IsValid()) {
        return FTransform::Identity;
    }
    auto const parent_index{session_state_->groups.IndexOfByPredicate(
        [parent_id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == parent_id; })};
    check(session_state_->groups.IsValidIndex(parent_index));
    return get_group_world_transform(parent_index);
}

void USbxMeshGenLabEditorMode::set_part_world_transform(int32 const part_index,
                                                        FTransform const& transform) {
    auto assembly_transform{transform};
    assembly_transform.AddToTranslation(-preview_origin_);
    auto& recipe_part{session_state_->parts[part_index]};
    auto const local_transform{
        assembly_transform.GetRelativeTransform(get_parent_world_transform(recipe_part.parent_id))};
    recipe_part.translation = local_transform.GetLocation();
    recipe_part.rotation = local_transform.Rotator();
    recipe_part.scale = local_transform.GetScale3D();
    parts_[part_index].transform = {FVector3f{assembly_transform.GetLocation()},
                                    FRotator3f{assembly_transform.Rotator()},
                                    FVector3f{assembly_transform.GetScale3D()}};
}

void USbxMeshGenLabEditorMode::set_group_world_transform(int32 const group_index,
                                                         FTransform const& transform) {
    auto assembly_transform{transform};
    assembly_transform.AddToTranslation(-preview_origin_);
    auto& group{session_state_->groups[group_index]};
    group.set_transform(
        assembly_transform.GetRelativeTransform(get_parent_world_transform(group.parent_id)));
}

auto USbxMeshGenLabEditorMode::get_descendant_group_indices(FGuid const group_id) const
    -> TArray<int32> {
    TArray<int32> indices;
    auto const group_count{session_state_->groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        auto ancestor_id{session_state_->groups[group_index].id};
        while (ancestor_id.IsValid()) {
            if (ancestor_id == group_id) {
                indices.Add(group_index);
                break;
            }
            auto const ancestor_index{session_state_->groups.IndexOfByPredicate(
                [ancestor_id](FSbxMeshAssemblyRecipeGroup const& group) {
                    return group.id == ancestor_id;
                })};
            if (!session_state_->groups.IsValidIndex(ancestor_index)) {
                break;
            }
            ancestor_id = session_state_->groups[ancestor_index].parent_id;
        }
    }
    return indices;
}

auto USbxMeshGenLabEditorMode::get_descendant_part_indices(FGuid const group_id) const
    -> TArray<int32> {
    TArray<int32> indices;
    auto const part_count{session_state_->parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto ancestor_id{session_state_->parts[part_index].parent_id};
        while (ancestor_id.IsValid()) {
            if (ancestor_id == group_id) {
                indices.Add(part_index);
                break;
            }
            auto const ancestor_index{session_state_->groups.IndexOfByPredicate(
                [ancestor_id](FSbxMeshAssemblyRecipeGroup const& group) {
                    return group.id == ancestor_id;
                })};
            if (!session_state_->groups.IsValidIndex(ancestor_index)) {
                break;
            }
            ancestor_id = session_state_->groups[ancestor_index].parent_id;
        }
    }
    return indices;
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

auto USbxMeshGenLabEditorMode::find_preview_part(
    UInstancedStaticMeshComponent const* const component, int32 const instance_index) const
    -> int32 {
    auto const bucket_index{
        preview_buckets_.IndexOfByPredicate([component](FSbxMeshPreviewBucket const& bucket) {
            return bucket.component.Get() == component;
        })};
    if (!preview_buckets_.IsValidIndex(bucket_index) ||
        !preview_buckets_[bucket_index].instance_part_ids.IsValidIndex(instance_index)) {
        return INDEX_NONE;
    }

    auto const part_id{preview_buckets_[bucket_index].instance_part_ids[instance_index]};
    auto const* const part_index{part_index_by_id_.Find(part_id)};
    return part_index == nullptr ? INDEX_NONE : *part_index;
}

auto USbxMeshGenLabEditorMode::get_preview_part_bounds(int32 const part_index) const -> FBox {
    if (!parts_.IsValidIndex(part_index) || !part_ids_.IsValidIndex(part_index)) {
        return FBox{ForceInit};
    }
    auto const* const location{preview_location_by_part_id_.Find(part_ids_[part_index])};
    if (location == nullptr || !preview_buckets_.IsValidIndex(location->bucket_index)) {
        return FBox{ForceInit};
    }
    auto const* const component{preview_buckets_[location->bucket_index].component.Get()};
    UStaticMesh const* const static_mesh{component == nullptr ? nullptr
                                                              : component->GetStaticMesh().Get()};
    if (static_mesh == nullptr) {
        return FBox{ForceInit};
    }
    return static_mesh->GetBounds()
        .TransformBy(make_part_world_transform(parts_[part_index]))
        .GetBox();
}

void USbxMeshGenLabEditorMode::apply_marquee_selection(TArray<int32> const& matching_part_indices,
                                                       bool const select) {
    auto selected_part_indices{selected_part_indices_};
    if (select) {
        for (int32 const part_index : matching_part_indices) {
            selected_part_indices.AddUnique(part_index);
        }
    } else {
        for (int32 const part_index : matching_part_indices) {
            selected_part_indices.Remove(part_index);
        }
    }

    if (selected_part_indices.IsEmpty()) {
        SelectNone();
        return;
    }
    auto const primary_part_index{select && !matching_part_indices.IsEmpty()
                                      ? matching_part_indices.Last()
                                      : selected_part_indices.Last()};
    select_parts(selected_part_indices, primary_part_index);
}

auto USbxMeshGenLabEditorMode::make_part_world_transform(FSbxMeshAssemblyPart const& part) const
    -> FTransform {
    return FTransform{FRotator{part.transform.rotation},
                      preview_origin_ + FVector{part.transform.translation},
                      FVector{part.transform.scale}};
}

#undef LOCTEXT_NAMESPACE
