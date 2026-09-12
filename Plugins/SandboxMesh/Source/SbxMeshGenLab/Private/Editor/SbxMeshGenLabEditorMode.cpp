#include "Editor/SbxMeshGenLabEditorMode.h"

#include "Editor/SbxMeshGenLabEditorModeToolkit.h"
#include "Generation/MeshAssemblyRecipeAsset.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
#include "SbxMeshGenLab/MeshAssemblyRecipeJson.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "HitProxies.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"
#include "SceneView.h"
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

auto get_json_recipes_directory() -> FString {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxMesh"))};
    return plugin.IsValid() ? FPaths::Combine(plugin->GetBaseDir(), TEXT("Recipes"))
                            : FPaths::ProjectDir();
}

auto get_dialog_parent_window() -> void const* {
    return FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
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

void USbxMeshGenLabEditorMode::Render(FSceneView const* const view,
                                      FViewport* const viewport,
                                      FPrimitiveDrawInterface* const primitive_draw_interface) {
    Super::Render(view, viewport, primitive_draw_interface);
    if (primitive_draw_interface == nullptr || session_state_ == nullptr) {
        return;
    }

    auto const draw_connector = [this, primitive_draw_interface](int32 const group_index,
                                                                 int32 const connector_index,
                                                                 FLinearColor const color) {
        if (!session_state_->groups.IsValidIndex(group_index) ||
            !session_state_->groups[group_index].connectors.IsValidIndex(connector_index)) {
            return;
        }
        auto transform{get_connector_world_transform(group_index, connector_index)};
        transform.AddToTranslation(preview_origin_);
        auto const origin{transform.GetLocation()};
        constexpr double axis_length{24.0};
        primitive_draw_interface->DrawPoint(origin, color, 10.0f, SDPG_Foreground);
        primitive_draw_interface->DrawLine(
            origin,
            origin + transform.TransformVectorNoScale(FVector::XAxisVector) * axis_length,
            color,
            SDPG_Foreground,
            2.0f);
        primitive_draw_interface->DrawLine(
            origin,
            origin + transform.TransformVectorNoScale(FVector::YAxisVector) * axis_length,
            FLinearColor::Green,
            SDPG_Foreground,
            1.5f);
        primitive_draw_interface->DrawLine(
            origin,
            origin + transform.TransformVectorNoScale(FVector::ZAxisVector) * axis_length,
            FLinearColor::Blue,
            SDPG_Foreground,
            1.5f);
    };

    if (session_state_->groups.IsValidIndex(selected_group_index_)) {
        auto const connector_count{session_state_->groups[selected_group_index_].connectors.Num()};
        for (int32 connector_index{}; connector_index < connector_count; ++connector_index) {
            draw_connector(selected_group_index_, connector_index, FLinearColor::Yellow);
        }
    }

    auto const target_group_index{
        session_state_->groups.IndexOfByPredicate([this](FSbxMeshAssemblyRecipeGroup const& group) {
            return group.id == snap_target_group_id_;
        })};
    draw_connector(
        target_group_index, snap_target_connector_index_, FLinearColor{0.0f, 1.0f, 1.0f});

    auto const source_connector_index{get_settings()->active_connector_index};
    if (session_state_->groups.IsValidIndex(selected_group_index_) &&
        session_state_->groups[selected_group_index_].connectors.IsValidIndex(
            source_connector_index) &&
        session_state_->groups.IsValidIndex(target_group_index) &&
        session_state_->groups[target_group_index].connectors.IsValidIndex(
            snap_target_connector_index_)) {
        auto source_transform{
            get_connector_world_transform(selected_group_index_, source_connector_index)};
        auto target_transform{
            get_connector_world_transform(target_group_index, snap_target_connector_index_)};
        source_transform.AddToTranslation(preview_origin_);
        target_transform.AddToTranslation(preview_origin_);
        primitive_draw_interface->DrawLine(source_transform.GetLocation(),
                                           target_transform.GetLocation(),
                                           FLinearColor{1.0f, 0.25f, 1.0f},
                                           SDPG_Foreground,
                                           2.0f);
    }
}

void USbxMeshGenLabEditorMode::DrawHUD(FEditorViewportClient* const viewport_client,
                                       FViewport* const viewport,
                                       FSceneView const* const view,
                                       FCanvas* const canvas) {
    Super::DrawHUD(viewport_client, viewport, view, canvas);
    if (session_state_ == nullptr || viewport == nullptr || view == nullptr || canvas == nullptr ||
        GEngine == nullptr) {
        return;
    }

    auto const viewport_size{viewport->GetSizeXY()};
    auto const draw_label = [this, view, canvas, viewport_size](int32 const group_index,
                                                                int32 const connector_index,
                                                                FText const& prefix,
                                                                FLinearColor const color) {
        if (!session_state_->groups.IsValidIndex(group_index) ||
            !session_state_->groups[group_index].connectors.IsValidIndex(connector_index)) {
            return;
        }
        auto transform{get_connector_world_transform(group_index, connector_index)};
        transform.AddToTranslation(preview_origin_);
        auto const projected{view->Project(transform.GetLocation())};
        if (projected.W <= 0.0) {
            return;
        }
        auto const& group{session_state_->groups[group_index]};
        auto const& connector{group.connectors[connector_index]};
        FCanvasTextItem text_item{
            FVector2D{viewport_size.X * 0.5 + viewport_size.X * 0.5 * projected.X,
                      viewport_size.Y * 0.5 - viewport_size.Y * 0.5 * projected.Y},
            FText::Format(LOCTEXT("ConnectorLabel", "{0}{1} / {2}"),
                          prefix,
                          FText::FromName(group.name),
                          FText::FromName(connector.name)),
            GEngine->GetSmallFont(),
            color};
        text_item.EnableShadow(FLinearColor::Black);
        canvas->DrawItem(text_item);
    };

    if (session_state_->groups.IsValidIndex(selected_group_index_)) {
        auto const connector_count{session_state_->groups[selected_group_index_].connectors.Num()};
        auto const active_connector_index{get_settings()->active_connector_index};
        for (int32 connector_index{}; connector_index < connector_count; ++connector_index) {
            draw_label(selected_group_index_,
                       connector_index,
                       connector_index == active_connector_index
                           ? LOCTEXT("SourceLabel", "Source: ")
                           : FText::GetEmpty(),
                       FLinearColor::Yellow);
        }
    }

    auto const target_group_index{
        session_state_->groups.IndexOfByPredicate([this](FSbxMeshAssemblyRecipeGroup const& group) {
            return group.id == snap_target_group_id_;
        })};
    draw_label(target_group_index,
               snap_target_connector_index_,
               LOCTEXT("TargetLabel", "Target: "),
               FLinearColor{0.0f, 1.0f, 1.0f});
}

auto USbxMeshGenLabEditorMode::UsesTransformWidget() const -> bool {
    if (selected_part_indices_.IsEmpty() || !parts_.IsValidIndex(selected_part_index_)) {
        return false;
    }
    if (selected_group_index_ != INDEX_NONE) {
        return !is_node_locked(session_state_->groups[selected_group_index_].id);
    }
    return !selected_part_indices_.ContainsByPredicate(
        [this](int32 const part_index) { return parts_[part_index].locked; });
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
    auto const primary_scale{
        selected_group_index_ != INDEX_NONE
            ? get_group_world_transform(selected_group_index_).GetScale3D()
            : SandboxMesh::to_unreal(parts_[selected_part_index_].transform.scale)};
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
        get_settings()->load_transform({SandboxMesh::to_native(group.translation),
                                        SandboxMesh::to_native(group.rotation),
                                        SandboxMesh::to_native(group.scale)});
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
        }
        rebuild_resolved_parts();
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

auto USbxMeshGenLabEditorMode::HasCustomViewportFocus() const -> bool {
    return !selected_part_indices_.IsEmpty();
}

auto USbxMeshGenLabEditorMode::ComputeCustomViewportFocus() const -> FBox {
    FBox bounds{ForceInit};
    for (int32 const part_index : selected_part_indices_) {
        bounds += get_preview_part_bounds(part_index);
    }
    return bounds;
}

auto USbxMeshGenLabEditorMode::get_settings() const -> USbxMeshGenLabSettings* {
    return CastChecked<USbxMeshGenLabSettings>(SettingsObject);
}

auto USbxMeshGenLabEditorMode::get_parts() const -> TArray<FSbxMeshAssemblyPart> const& {
    return parts_;
}

auto USbxMeshGenLabEditorMode::get_recipe_parts() const
    -> TArray<FSbxMeshAssemblyRecipePart> const& {
    return session_state_->parts;
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
    return !selected_part_indices_.IsEmpty() && selected_part_indices_.Num() < parts_.Num() &&
           !selected_part_indices_.ContainsByPredicate(
               [this](int32 const part_index) { return parts_[part_index].locked; });
}

auto USbxMeshGenLabEditorMode::can_create_group() const -> bool {
    return (selected_group_index_ != INDEX_NONE || !selected_part_indices_.IsEmpty()) &&
           !selected_part_indices_.ContainsByPredicate(
               [this](int32 const part_index) { return parts_[part_index].locked; });
}

auto USbxMeshGenLabEditorMode::can_ungroup() const -> bool {
    return session_state_ != nullptr &&
           session_state_->groups.IsValidIndex(selected_group_index_) &&
           !selected_part_indices_.ContainsByPredicate(
               [this](int32 const part_index) { return parts_[part_index].locked; });
}

auto USbxMeshGenLabEditorMode::can_set_snap_target() const -> bool {
    if (session_state_ == nullptr || !session_state_->groups.IsValidIndex(selected_group_index_)) {
        return false;
    }
    return session_state_->groups[selected_group_index_].connectors.IsValidIndex(
        get_settings()->active_connector_index);
}

auto USbxMeshGenLabEditorMode::can_snap_selected_group() const -> bool {
    if (!can_set_snap_target()) {
        return false;
    }
    auto const target_group_index{
        session_state_->groups.IndexOfByPredicate([this](FSbxMeshAssemblyRecipeGroup const& group) {
            return group.id == snap_target_group_id_;
        })};
    if (!session_state_->groups.IsValidIndex(target_group_index) ||
        target_group_index == selected_group_index_ ||
        is_node_locked(session_state_->groups[selected_group_index_].id) ||
        !session_state_->groups[target_group_index].connectors.IsValidIndex(
            snap_target_connector_index_)) {
        return false;
    }
    auto const selected_group_id{session_state_->groups[selected_group_index_].id};
    return !get_descendant_group_indices(selected_group_id).Contains(target_group_index);
}

auto USbxMeshGenLabEditorMode::get_status() const -> FText const& {
    return status_;
}

auto USbxMeshGenLabEditorMode::get_snap_target_text() const -> FText {
    if (session_state_ == nullptr) {
        return LOCTEXT("NoSnapTarget", "Snap target: not set");
    }
    auto const target_group_index{
        session_state_->groups.IndexOfByPredicate([this](FSbxMeshAssemblyRecipeGroup const& group) {
            return group.id == snap_target_group_id_;
        })};
    if (!session_state_->groups.IsValidIndex(target_group_index) ||
        !session_state_->groups[target_group_index].connectors.IsValidIndex(
            snap_target_connector_index_)) {
        return LOCTEXT("NoSnapTarget", "Snap target: not set");
    }
    auto const& group{session_state_->groups[target_group_index]};
    return FText::Format(LOCTEXT("CurrentSnapTarget", "Snap target: {0} / {1}"),
                         FText::FromName(group.name),
                         FText::FromName(group.connectors[snap_target_connector_index_].name));
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
    settings->group_connectors.Reset();
    settings->active_connector_index = 0;
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
    get_settings()->load_transform(
        {SandboxMesh::to_native(session_state_->groups[group_index].translation),
         SandboxMesh::to_native(session_state_->groups[group_index].rotation),
         SandboxMesh::to_native(session_state_->groups[group_index].scale)});
    get_settings()->group_connectors = session_state_->groups[group_index].connectors;
    get_settings()->active_connector_index =
        FMath::Clamp(get_settings()->active_connector_index,
                     0,
                     FMath::Max(0, get_settings()->group_connectors.Num() - 1));
    select_preview_instances();
    status_ = FText::Format(LOCTEXT("GroupSelected", "Selected group '{0}' ({1} parts)."),
                            FText::FromName(session_state_->groups[group_index].name),
                            FText::AsNumber(part_indices.Num()));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::select_node(FGuid const id) {
    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (group_index != INDEX_NONE) {
        select_group(group_index);
        return;
    }

    auto const* const part_index{part_index_by_id_.Find(id)};
    if (part_index != nullptr) {
        select_part(*part_index);
    }
}

void USbxMeshGenLabEditorMode::select_nodes(TArray<FGuid> const& ids, FGuid const primary_id) {
    if (ids.Num() == 1) {
        select_node(ids[0]);
        return;
    }

    TArray<int32> part_indices;
    for (FGuid const id : ids) {
        if (auto const* const part_index{part_index_by_id_.Find(id)}; part_index != nullptr) {
            part_indices.AddUnique(*part_index);
            continue;
        }

        auto const group_index{session_state_->groups.IndexOfByPredicate(
            [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
        if (session_state_->groups.IsValidIndex(group_index)) {
            for (int32 const part_index : get_descendant_part_indices(id)) {
                part_indices.AddUnique(part_index);
            }
        }
    }
    if (part_indices.IsEmpty()) {
        return;
    }

    auto const* const primary_index{part_index_by_id_.Find(primary_id)};
    select_parts(part_indices, primary_index == nullptr ? part_indices.Last() : *primary_index);
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
    auto recipe_part{FSbxMeshAssemblyRecipePart::from_part(part, part_id)};
    recipe_part.name = FName{FString::Printf(TEXT("Part %d"), session_state_->parts.Num() + 1)};
    session_state_->parts.Add(MoveTemp(recipe_part));
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
    if (selected_part_indices_.ContainsByPredicate(
            [this](int32 const part_index) { return parts_[part_index].locked; })) {
        status_ = LOCTEXT("LockedDuplicateRejected", "Unlock the selection before duplicating it.");
        notify_session_changed(false);
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
            FTransform transform{SandboxMesh::to_unreal(part.transform.rotation),
                                 SandboxMesh::to_unreal(part.transform.translation),
                                 SandboxMesh::to_unreal(part.transform.scale)};
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
            part.transform = {SandboxMesh::to_native(local_transform.GetLocation()),
                              SandboxMesh::to_native(local_transform.Rotator()),
                              SandboxMesh::to_native(local_transform.GetScale3D())};
            if (part_index == selected_part_index_ && repeat_index == repeat_count) {
                duplicate_primary_offset = duplicate_parts.Num();
            }
            auto duplicate_part{
                FSbxMeshAssemblyRecipePart::from_part(part, FGuid::NewGuid(), parent_id)};
            auto const& source_part{session_state_->parts[part_index]};
            duplicate_part.name =
                FName{FString::Printf(TEXT("%s Copy"), *source_part.name.ToString())};
            duplicate_part.visible = source_part.visible;
            duplicate_part.locked = source_part.locked;
            duplicate_parts.Add(MoveTemp(duplicate_part));
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
    TSet<FGuid> node_ids_to_remove{ids_to_remove};
    if (selected_group_index_ != INDEX_NONE) {
        for (int32 const group_index :
             get_descendant_group_indices(session_state_->groups[selected_group_index_].id)) {
            node_ids_to_remove.Add(session_state_->groups[group_index].id);
        }
    }
    TArray<int32> group_indices_to_remove;
    auto const group_count{session_state_->groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        if (node_ids_to_remove.Contains(session_state_->groups[group_index].id)) {
            group_indices_to_remove.Add(group_index);
        }
    }
    group_indices_to_remove.Sort([](int32 const left, int32 const right) { return left > right; });

    auto const find_surviving_parent = [this, &node_ids_to_remove](FGuid parent_id) {
        while (parent_id.IsValid() && node_ids_to_remove.Contains(parent_id)) {
            parent_id = get_node_parent_id(parent_id);
        }
        return parent_id;
    };
    for (auto& group : session_state_->groups) {
        if (node_ids_to_remove.Contains(group.id) ||
            !node_ids_to_remove.Contains(group.parent_id)) {
            continue;
        }
        auto const world_transform{get_node_world_transform(group.id)};
        group.parent_id = find_surviving_parent(group.parent_id);
        group.set_transform(
            world_transform.GetRelativeTransform(get_parent_world_transform(group.parent_id)));
    }
    auto const recipe_part_count{session_state_->parts.Num()};
    for (int32 part_index{}; part_index < recipe_part_count; ++part_index) {
        auto& part{session_state_->parts[part_index]};
        if (node_ids_to_remove.Contains(part.id) || !node_ids_to_remove.Contains(part.parent_id)) {
            continue;
        }
        auto const world_transform{get_node_world_transform(part.id)};
        part.parent_id = find_surviving_parent(part.parent_id);
        auto const local_transform{
            world_transform.GetRelativeTransform(get_parent_world_transform(part.parent_id))};
        part.translation = local_transform.GetLocation();
        part.rotation = local_transform.Rotator();
        part.scale = local_transform.GetScale3D();
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
    rebuild_part_index_map();
    for (int32 const group_index : group_indices_to_remove) {
        session_state_->groups.RemoveAt(group_index);
    }

    for (int32 group_index{session_state_->groups.Num() - 1}; group_index >= 0; --group_index) {
        if (get_descendant_part_indices(session_state_->groups[group_index].id).IsEmpty()) {
            session_state_->groups.RemoveAt(group_index);
        }
    }
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
    FSbxMeshAssemblyConnector connector;
    connector.name = TEXT("Origin");
    group.connectors.Add(connector);

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
            pivot += SandboxMesh::to_unreal(parts_[part_index].transform.translation);
        }
        pivot /= selected_part_indices_.Num();
        FTransform const group_world{FQuat::Identity, pivot, FVector::OneVector};
        group.set_transform(
            group_world.GetRelativeTransform(get_parent_world_transform(group.parent_id)));

        for (int32 const part_index : selected_part_indices_) {
            auto& recipe_part{session_state_->parts[part_index]};
            FTransform const part_world{
                SandboxMesh::to_unreal(parts_[part_index].transform.rotation),
                SandboxMesh::to_unreal(parts_[part_index].transform.translation),
                SandboxMesh::to_unreal(parts_[part_index].transform.scale)};
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
        FTransform const part_world{
            SandboxMesh::to_unreal(parts_[part_index].transform.rotation),
            SandboxMesh::to_unreal(parts_[part_index].transform.translation),
            SandboxMesh::to_unreal(parts_[part_index].transform.scale)};
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

void USbxMeshGenLabEditorMode::set_snap_target() {
    if (!can_set_snap_target()) {
        return;
    }
    snap_target_group_id_ = session_state_->groups[selected_group_index_].id;
    snap_target_connector_index_ = get_settings()->active_connector_index;
    auto const& connector{
        session_state_->groups[selected_group_index_].connectors[snap_target_connector_index_]};
    status_ = FText::Format(LOCTEXT("SnapTargetSet", "Snap target: {0} / {1}."),
                            FText::FromName(session_state_->groups[selected_group_index_].name),
                            FText::FromName(connector.name));
    notify_session_changed(false);
}

void USbxMeshGenLabEditorMode::align_connectors() {
    apply_connector_snap(false);
}

void USbxMeshGenLabEditorMode::snap_and_parent() {
    apply_connector_snap(true);
}

void USbxMeshGenLabEditorMode::apply_connector_snap(bool const parent_to_target) {
    if (!can_snap_selected_group()) {
        return;
    }

    auto const target_group_index{
        session_state_->groups.IndexOfByPredicate([this](FSbxMeshAssemblyRecipeGroup const& group) {
            return group.id == snap_target_group_id_;
        })};
    auto const source_connector_index{get_settings()->active_connector_index};
    auto const source_connector{session_state_->groups[selected_group_index_]
                                    .connectors[source_connector_index]
                                    .to_transform()};
    auto target_connector{
        get_connector_world_transform(target_group_index, snap_target_connector_index_)};
    if (get_settings()->connectors_face_to_face) {
        FTransform const facing_rotation{FRotator{0.0, 180.0, 0.0}};
        target_connector = facing_rotation * target_connector;
    }
    auto const current_group_world{get_group_world_transform(selected_group_index_)};
    auto desired_group_world{source_connector.Inverse() * target_connector};
    desired_group_world.SetScale3D(current_group_world.GetScale3D());
    desired_group_world.SetLocation(
        target_connector.GetLocation() -
        desired_group_world.TransformVector(source_connector.GetLocation()));
    desired_group_world.NormalizeRotation();

    auto preview_transform{desired_group_world};
    preview_transform.AddToTranslation(preview_origin_);
    if (!is_safe_preview_transform(preview_transform)) {
        status_ =
            LOCTEXT("UnsafeSnapTransform", "Snapping would create an invalid group transform.");
        notify_session_changed(false);
        return;
    }

    FScopedTransaction const transaction{
        parent_to_target
            ? LOCTEXT("SnapAndParentAssemblyGroupTransaction", "Snap and Parent Mesh Group")
            : LOCTEXT("AlignAssemblyGroupTransaction", "Align Mesh Group Connectors")};
    session_state_->Modify();
    if (parent_to_target) {
        auto& selected_group{session_state_->groups[selected_group_index_]};
        selected_group.parent_id = session_state_->groups[target_group_index].id;
        selected_group.set_transform(desired_group_world.GetRelativeTransform(
            get_group_world_transform(target_group_index)));
    } else {
        set_group_world_transform(selected_group_index_, preview_transform);
    }
    rebuild_resolved_parts();
    auto const& group{session_state_->groups[selected_group_index_]};
    get_settings()->load_transform({SandboxMesh::to_native(group.translation),
                                    SandboxMesh::to_native(group.rotation),
                                    SandboxMesh::to_native(group.scale)});
    mark_recipe_dirty();
    status_ =
        FText::Format(parent_to_target ? LOCTEXT("GroupSnappedAndParented",
                                                 "Snapped group '{0}' and parented it to '{1}'.")
                                       : LOCTEXT("GroupAligned", "Aligned group '{0}' to '{1}'."),
                      FText::FromName(group.name),
                      FText::FromName(session_state_->groups[target_group_index].name));
    notify_session_changed();
}

auto USbxMeshGenLabEditorMode::rename_node(FGuid const id, FName const name) -> bool {
    if (name.IsNone() || is_node_locked(id)) {
        return false;
    }
    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    auto const* const part_index{part_index_by_id_.Find(id)};
    if (!session_state_->groups.IsValidIndex(group_index) && part_index == nullptr) {
        return false;
    }

    FScopedTransaction const transaction{
        LOCTEXT("RenameAssemblyNodeTransaction", "Rename Mesh Node")};
    session_state_->Modify();
    if (session_state_->groups.IsValidIndex(group_index)) {
        session_state_->groups[group_index].name = name;
    } else {
        session_state_->parts[*part_index].name = name;
    }
    mark_recipe_dirty();
    status_ =
        FText::Format(LOCTEXT("NodeRenamed", "Renamed node to '{0}'."), FText::FromName(name));
    notify_session_changed();
    return true;
}

void USbxMeshGenLabEditorMode::toggle_node_visibility(FGuid const id) {
    FScopedTransaction const transaction{
        LOCTEXT("ToggleAssemblyNodeVisibilityTransaction", "Toggle Mesh Node Visibility")};
    session_state_->Modify();

    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (session_state_->groups.IsValidIndex(group_index)) {
        session_state_->groups[group_index].visible = !session_state_->groups[group_index].visible;
    } else if (auto const* const part_index{part_index_by_id_.Find(id)}; part_index != nullptr) {
        session_state_->parts[*part_index].visible = !session_state_->parts[*part_index].visible;
    } else {
        return;
    }

    rebuild_resolved_parts(true);
    mark_recipe_dirty();
    select_node(id);
    status_ = is_node_locally_visible(id) ? LOCTEXT("NodeShown", "Node is visible.")
                                          : LOCTEXT("NodeHidden", "Node is hidden.");
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::toggle_node_lock(FGuid const id) {
    FScopedTransaction const transaction{
        LOCTEXT("ToggleAssemblyNodeLockTransaction", "Toggle Mesh Node Lock")};
    session_state_->Modify();

    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (session_state_->groups.IsValidIndex(group_index)) {
        session_state_->groups[group_index].locked = !session_state_->groups[group_index].locked;
    } else if (auto const* const part_index{part_index_by_id_.Find(id)}; part_index != nullptr) {
        session_state_->parts[*part_index].locked = !session_state_->parts[*part_index].locked;
    } else {
        return;
    }

    rebuild_resolved_parts();
    mark_recipe_dirty();
    select_node(id);
    status_ = is_node_locally_locked(id) ? LOCTEXT("NodeLocked", "Node is locked.")
                                         : LOCTEXT("NodeUnlocked", "Node is unlocked.");
    notify_session_changed();
}

auto USbxMeshGenLabEditorMode::is_node_locally_visible(FGuid const id) const -> bool {
    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (session_state_->groups.IsValidIndex(group_index)) {
        return session_state_->groups[group_index].visible;
    }
    auto const* const part_index{part_index_by_id_.Find(id)};
    return part_index != nullptr && session_state_->parts[*part_index].visible;
}

auto USbxMeshGenLabEditorMode::is_node_locally_locked(FGuid const id) const -> bool {
    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (session_state_->groups.IsValidIndex(group_index)) {
        return session_state_->groups[group_index].locked;
    }
    auto const* const part_index{part_index_by_id_.Find(id)};
    return part_index != nullptr && session_state_->parts[*part_index].locked;
}

auto USbxMeshGenLabEditorMode::can_reparent_nodes(TArray<FGuid> const& ids,
                                                  FGuid const parent_id) const -> bool {
    if (ids.IsEmpty()) {
        return false;
    }
    if (parent_id.IsValid() && !part_index_by_id_.Contains(parent_id) &&
        !session_state_->groups.ContainsByPredicate(
            [parent_id](FSbxMeshAssemblyRecipeGroup const& group) {
                return group.id == parent_id;
            })) {
        return false;
    }

    for (FGuid const id : ids) {
        auto const node_exists{
            part_index_by_id_.Contains(id) ||
            session_state_->groups.ContainsByPredicate(
                [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
        if (!node_exists || is_node_locked(id) || id == parent_id ||
            is_node_descendant(parent_id, id)) {
            return false;
        }
    }
    if (parent_id.IsValid() && is_node_locked(parent_id)) {
        return false;
    }
    return true;
}

auto USbxMeshGenLabEditorMode::reparent_node(FGuid const id, FGuid const parent_id) -> bool {
    return reparent_nodes({id}, parent_id);
}

auto USbxMeshGenLabEditorMode::reparent_nodes(TArray<FGuid> const& ids, FGuid const parent_id)
    -> bool {
    if (!can_reparent_nodes(ids, parent_id)) {
        status_ = LOCTEXT("CyclicHierarchyRejected",
                          "That move would create an invalid hierarchy or parenting cycle.");
        notify_session_changed(false);
        return false;
    }

    TArray<FGuid> root_ids;
    for (FGuid const id : ids) {
        auto const has_selected_ancestor{ids.ContainsByPredicate([this, id](FGuid const candidate) {
            return candidate != id && is_node_descendant(id, candidate);
        })};
        if (!has_selected_ancestor) {
            root_ids.AddUnique(id);
        }
    }

    auto const parent_world{get_parent_world_transform(parent_id)};
    TMap<FGuid, FTransform> world_transforms;
    for (FGuid const id : root_ids) {
        world_transforms.Add(id, get_node_world_transform(id));
    }

    FScopedTransaction const transaction{
        LOCTEXT("ReparentAssemblyNodesTransaction", "Move Mesh Hierarchy Nodes")};
    session_state_->Modify();

    for (FGuid const id : root_ids) {
        auto const local_transform{
            world_transforms.FindChecked(id).GetRelativeTransform(parent_world)};
        auto const group_index{session_state_->groups.IndexOfByPredicate(
            [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
        if (session_state_->groups.IsValidIndex(group_index)) {
            auto& group{session_state_->groups[group_index]};
            group.parent_id = parent_id;
            group.set_transform(local_transform);
            continue;
        }

        auto& part{session_state_->parts[part_index_by_id_.FindChecked(id)]};
        part.parent_id = parent_id;
        part.translation = local_transform.GetLocation();
        part.rotation = local_transform.Rotator();
        part.scale = local_transform.GetScale3D();
    }

    rebuild_resolved_parts();
    mark_recipe_dirty();
    status_ = FText::Format(
        LOCTEXT("HierarchyNodesMoved", "Parented {0} node(s) without changing world transforms."),
        FText::AsNumber(root_ids.Num()));
    notify_session_changed();
    return true;
}

void USbxMeshGenLabEditorMode::new_assembly() {
    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
    destroy_preview();
    snap_target_group_id_.Invalidate();
    snap_target_connector_index_ = INDEX_NONE;

    auto* const settings{get_settings()};
    settings->load_request(SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box));
    settings->load_transform({});
    settings->group_connectors.Reset();
    settings->active_connector_index = 0;
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
    auto recipe_part{FSbxMeshAssemblyRecipePart::from_part(part)};
    recipe_part.name = TEXT("Part 1");
    session_state_->parts = {MoveTemp(recipe_part)};
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
        part.mesh.asset_name = SandboxMesh::to_native(settings->asset_name);
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
    if (selected_recipe->format_version != 1 && selected_recipe->format_version != 2 &&
        selected_recipe->format_version != 3) {
        status_ = FText::Format(
            LOCTEXT("RecipeVersionUnsupported", "Recipe format version {0} is not supported."),
            FText::AsNumber(selected_recipe->format_version));
        notify_session_changed();
        return;
    }
    snap_target_group_id_.Invalidate();
    snap_target_connector_index_ = INDEX_NONE;

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
    replace_session_from_recipe(MoveTemp(recipe_parts),
                                MoveTemp(recipe_groups),
                                selected_recipe->output_asset_name,
                                selected_recipe->GetFName(),
                                selected_recipe,
                                false,
                                FText::Format(LOCTEXT("RecipeLoaded", "Loaded recipe {0}."),
                                              FText::FromString(selected_recipe->GetPathName())));
}

void USbxMeshGenLabEditorMode::export_recipe_json() {
    apply_settings(false);

    auto* const desktop_platform{FDesktopPlatformModule::Get()};
    if (desktop_platform == nullptr) {
        status_ = LOCTEXT("JsonExportUnavailable", "The desktop file dialog is unavailable.");
        notify_session_changed();
        return;
    }

    auto* const settings{get_settings()};
    auto const default_filename{settings->recipe_name.ToString() + TEXT(".json")};
    TArray<FString> filenames;
    if (!desktop_platform->SaveFileDialog(get_dialog_parent_window(),
                                          TEXT("Export Sandbox Mesh Recipe"),
                                          get_json_recipes_directory(),
                                          default_filename,
                                          TEXT("Sandbox Mesh recipe (*.json)|*.json"),
                                          EFileDialogFlags::None,
                                          filenames) ||
        filenames.IsEmpty()) {
        return;
    }

    FSbxMeshAssemblyRecipeJsonDocument document;
    document.recipe_name = settings->recipe_name;
    document.output_asset_name = settings->asset_name;
    document.parts = session_state_->parts;
    document.groups = session_state_->groups;
    FString error;
    if (!SandboxMesh::save_mesh_assembly_recipe_json(filenames[0], document, error)) {
        status_ = FText::FromString(error);
        notify_session_changed();
        return;
    }

    status_ = FText::Format(LOCTEXT("JsonExported", "Exported JSON recipe to {0}."),
                            FText::FromString(filenames[0]));
    notify_session_changed();
}

void USbxMeshGenLabEditorMode::import_recipe_json() {
    auto* const desktop_platform{FDesktopPlatformModule::Get()};
    if (desktop_platform == nullptr) {
        status_ = LOCTEXT("JsonImportUnavailable", "The desktop file dialog is unavailable.");
        notify_session_changed();
        return;
    }

    TArray<FString> filenames;
    if (!desktop_platform->OpenFileDialog(get_dialog_parent_window(),
                                          TEXT("Import Sandbox Mesh Recipe"),
                                          get_json_recipes_directory(),
                                          FString{},
                                          TEXT("Sandbox Mesh recipe (*.json)|*.json"),
                                          EFileDialogFlags::None,
                                          filenames) ||
        filenames.IsEmpty()) {
        return;
    }

    FSbxMeshAssemblyRecipeJsonDocument document;
    FString error;
    if (!SandboxMesh::load_mesh_assembly_recipe_json(filenames[0], document, error)) {
        status_ = FText::FromString(error);
        notify_session_changed();
        return;
    }

    replace_session_from_recipe(
        MoveTemp(document.parts),
        MoveTemp(document.groups),
        document.output_asset_name,
        document.recipe_name,
        nullptr,
        true,
        FText::Format(LOCTEXT("JsonImported", "Imported JSON recipe from {0}."),
                      FText::FromString(filenames[0])));
}

auto USbxMeshGenLabEditorMode::replace_session_from_recipe(
    TArray<FSbxMeshAssemblyRecipePart> recipe_parts,
    TArray<FSbxMeshAssemblyRecipeGroup> recipe_groups,
    FName const output_asset_name,
    FName const recipe_name,
    USbxMeshAssemblyRecipe* const current_recipe,
    bool const dirty,
    FText const& success_status) -> bool {
    auto const hierarchy_error{
        SandboxMesh::validate_mesh_assembly_hierarchy(recipe_parts, recipe_groups)};
    if (!hierarchy_error.IsEmpty()) {
        status_ = FText::FromString(hierarchy_error);
        notify_session_changed();
        return false;
    }
    auto parts{SandboxMesh::resolve_mesh_assembly_hierarchy(
        recipe_parts, recipe_groups, output_asset_name)};
    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        status_ = FText::FromString(validation_error);
        notify_session_changed();
        return false;
    }

    if (GEditor != nullptr) {
        changing_selection_ = true;
        GEditor->SelectNone(false, true, false);
        changing_selection_ = false;
    }
    destroy_preview();
    snap_target_group_id_.Invalidate();
    snap_target_connector_index_ = INDEX_NONE;
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

    auto* const settings{get_settings()};
    settings->asset_name = output_asset_name;
    settings->recipe_name = recipe_name;
    settings->recipe = current_recipe;
    current_recipe_ = current_recipe;

    auto const part_count{parts_.Num()};
    for (int32 part_index{0}; part_index < part_count; ++part_index) {
        add_preview_instance(part_index);
    }
    selected_part_index_ = INDEX_NONE;
    selected_part_indices_.Reset();
    selected_group_index_ = INDEX_NONE;
    select_part(0);
    status_ = success_status;
    recipe_dirty_ = dirty;
    notify_session_changed();
    return true;
}

void USbxMeshGenLabEditorMode::apply_settings() {
    apply_settings(true);
}

void USbxMeshGenLabEditorMode::apply_settings(bool const mark_dirty) {
    if (!parts_.IsValidIndex(selected_part_index_)) {
        return;
    }
    auto const selected_id{selected_group_index_ != INDEX_NONE
                               ? session_state_->groups[selected_group_index_].id
                               : session_state_->parts[selected_part_index_].id};
    if (is_node_locked(selected_id)) {
        status_ = LOCTEXT("LockedEditRejected", "Unlock the node before editing it.");
        select_node(selected_id);
        notify_session_changed();
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
        group.connectors = settings->group_connectors;
        rebuild_resolved_parts();
    } else {
        auto& recipe_part{session_state_->parts[selected_part_index_]};
        auto const part_id{recipe_part.id};
        auto const parent_id{recipe_part.parent_id};
        auto const name{recipe_part.name};
        auto const visible{recipe_part.visible};
        auto const locked{recipe_part.locked};
        recipe_part = FSbxMeshAssemblyRecipePart::from_part(
            {settings->to_request(), settings->to_transform()}, part_id, parent_id);
        recipe_part.name = name;
        recipe_part.visible = visible;
        recipe_part.locked = locked;
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
        part.mesh.asset_name = SandboxMesh::to_native(settings->asset_name);
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
    if (!parts_[part_index].visible) {
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
        for (int32 const part_index : source_part_indices) {
            duplicated_ids.Add(session_state_->parts[part_index].id, FGuid::NewGuid());
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
            auto const source_id{part.id};
            part.id = duplicated_ids.FindChecked(source_id);
            if (auto const* const duplicated_parent_id{duplicated_ids.Find(part.parent_id)};
                duplicated_parent_id != nullptr) {
                part.parent_id = *duplicated_parent_id;
            }
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
    return get_node_world_transform(session_state_->groups[group_index].id);
}

auto USbxMeshGenLabEditorMode::get_node_world_transform(FGuid const id) const -> FTransform {
    FTransform transform{FTransform::Identity};
    auto node_id{id};
    TSet<FGuid> visited_ids;
    while (node_id.IsValid()) {
        check(!visited_ids.Contains(node_id));
        visited_ids.Add(node_id);

        auto const group_index{session_state_->groups.IndexOfByPredicate(
            [node_id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == node_id; })};
        if (session_state_->groups.IsValidIndex(group_index)) {
            transform *= session_state_->groups[group_index].to_transform();
            node_id = session_state_->groups[group_index].parent_id;
            continue;
        }

        auto const* const part_index{part_index_by_id_.Find(node_id)};
        check(part_index != nullptr);
        auto const part{session_state_->parts[*part_index].to_part(NAME_None)};
        transform *= FTransform{SandboxMesh::to_unreal(part.transform.rotation),
                                SandboxMesh::to_unreal(part.transform.translation),
                                SandboxMesh::to_unreal(part.transform.scale)};
        node_id = session_state_->parts[*part_index].parent_id;
    }
    return transform;
}

auto USbxMeshGenLabEditorMode::get_node_parent_id(FGuid const id) const -> FGuid {
    auto const group_index{session_state_->groups.IndexOfByPredicate(
        [id](FSbxMeshAssemblyRecipeGroup const& group) { return group.id == id; })};
    if (session_state_->groups.IsValidIndex(group_index)) {
        return session_state_->groups[group_index].parent_id;
    }
    if (auto const* const part_index{part_index_by_id_.Find(id)}; part_index != nullptr) {
        return session_state_->parts[*part_index].parent_id;
    }
    return {};
}

auto USbxMeshGenLabEditorMode::is_node_descendant(FGuid const id, FGuid const ancestor_id) const
    -> bool {
    if (!id.IsValid() || !ancestor_id.IsValid()) {
        return false;
    }

    auto node_id{id};
    TSet<FGuid> visited_ids;
    while (node_id.IsValid() && !visited_ids.Contains(node_id)) {
        if (node_id == ancestor_id) {
            return true;
        }
        visited_ids.Add(node_id);
        node_id = get_node_parent_id(node_id);
    }
    return false;
}

auto USbxMeshGenLabEditorMode::is_node_locked(FGuid const id) const -> bool {
    auto node_id{id};
    TSet<FGuid> visited_ids;
    while (node_id.IsValid() && !visited_ids.Contains(node_id)) {
        visited_ids.Add(node_id);
        if (is_node_locally_locked(node_id)) {
            return true;
        }
        node_id = get_node_parent_id(node_id);
    }
    return false;
}

auto USbxMeshGenLabEditorMode::get_connector_world_transform(int32 const group_index,
                                                             int32 const connector_index) const
    -> FTransform {
    check(session_state_->groups.IsValidIndex(group_index));
    check(session_state_->groups[group_index].connectors.IsValidIndex(connector_index));
    return session_state_->groups[group_index].connectors[connector_index].to_transform() *
           get_group_world_transform(group_index);
}

auto USbxMeshGenLabEditorMode::get_parent_world_transform(FGuid const parent_id) const
    -> FTransform {
    if (!parent_id.IsValid()) {
        return FTransform::Identity;
    }
    return get_node_world_transform(parent_id);
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
    parts_[part_index].transform = {SandboxMesh::to_native(assembly_transform.GetLocation()),
                                    SandboxMesh::to_native(assembly_transform.Rotator()),
                                    SandboxMesh::to_native(assembly_transform.GetScale3D())};
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
        if (is_node_descendant(session_state_->groups[group_index].id, group_id)) {
            indices.Add(group_index);
        }
    }
    return indices;
}

auto USbxMeshGenLabEditorMode::get_descendant_part_indices(FGuid const group_id) const
    -> TArray<int32> {
    TArray<int32> indices;
    auto const part_count{session_state_->parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        if (is_node_descendant(session_state_->parts[part_index].id, group_id)) {
            indices.Add(part_index);
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
    return FTransform{SandboxMesh::to_unreal(part.transform.rotation),
                      preview_origin_ + SandboxMesh::to_unreal(part.transform.translation),
                      SandboxMesh::to_unreal(part.transform.scale)};
}

#undef LOCTEXT_NAMESPACE
