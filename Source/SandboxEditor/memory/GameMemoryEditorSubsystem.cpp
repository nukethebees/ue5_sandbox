#include "GameMemoryEditorSubsystem.h"

#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

#include <Editor.h>
#include <Editor/EditorEngine.h>

void USandboxEditorGameMemorySubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    backing_ = FGameMemoryBacking::create(FGameMemoryConfig::default_root_capacity_bytes);
    FGameMemoryBootstrap::acquire_backing_delegate().BindUObject(
        this, &USandboxEditorGameMemorySubsystem::acquire_backing);
    FEditorDelegates::OnEditorPreExit.AddUObject(
        this, &USandboxEditorGameMemorySubsystem::handle_editor_pre_exit);
}

void USandboxEditorGameMemorySubsystem::Deinitialize() {
    auto& delegate{FGameMemoryBootstrap::acquire_backing_delegate()};
    if (delegate.IsBoundToObject(this)) {
        delegate.Unbind();
    }

    FEditorDelegates::OnEditorPreExit.RemoveAll(this);
    end_active_pie_if_leased();
    checkf(!backing_.IsValid() || !backing_->is_leased(),
           TEXT("Editor game-memory backing is still leased during subsystem teardown."));
    backing_.Reset();

    Super::Deinitialize();
}

void USandboxEditorGameMemorySubsystem::handle_editor_pre_exit() {
    end_active_pie_if_leased();
}

void USandboxEditorGameMemorySubsystem::end_active_pie_if_leased() {
    if (!backing_.IsValid() || !backing_->is_leased()) {
        return;
    }

    if (IsValid(GEditor) && GEditor->PlayWorld != nullptr) {
        GEditor->EndPlayMap();
    }
}

auto USandboxEditorGameMemorySubsystem::acquire_backing() -> TOptional<FGameMemoryBackingLease> {
    return backing_.IsValid() ? backing_->try_acquire_lease() : NullOpt;
}

auto USandboxEditorGameMemorySubsystem::backing_address() const noexcept -> std::byte* {
    return backing_.IsValid() ? backing_->data() : nullptr;
}
