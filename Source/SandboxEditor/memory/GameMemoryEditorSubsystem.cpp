#include "GameMemoryEditorSubsystem.h"

#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

#include <Editor.h>
#include <Editor/EditorEngine.h>

void USandboxEditorGameMemorySubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    backing_ = std::make_unique<ioj::memory::RetainedBackingResource>(
        ioj::memory::Config::default_root_capacity_bytes);
    FGameMemoryBootstrap::upstream_resource_delegate().BindUObject(
        this, &USandboxEditorGameMemorySubsystem::upstream_resource);
    FEditorDelegates::OnEditorPreExit.AddUObject(
        this, &USandboxEditorGameMemorySubsystem::handle_editor_pre_exit);
}

void USandboxEditorGameMemorySubsystem::Deinitialize() {
    auto& delegate{FGameMemoryBootstrap::upstream_resource_delegate()};
    if (delegate.IsBoundToObject(this)) {
        delegate.Unbind();
    }

    FEditorDelegates::OnEditorPreExit.RemoveAll(this);
    end_active_pie_if_memory_in_use();
    checkf(!static_cast<bool>(backing_) || !backing_->is_in_use(),
           TEXT("Editor game-memory backing is still in use during subsystem teardown."));
    backing_.reset();

    Super::Deinitialize();
}

void USandboxEditorGameMemorySubsystem::handle_editor_pre_exit() {
    end_active_pie_if_memory_in_use();
}

void USandboxEditorGameMemorySubsystem::end_active_pie_if_memory_in_use() {
    if (!static_cast<bool>(backing_) || !backing_->is_in_use()) {
        return;
    }

    if (IsValid(GEditor) && GEditor->PlayWorld != nullptr) {
        GEditor->EndPlayMap();
    }
}

auto USandboxEditorGameMemorySubsystem::upstream_resource(std::size_t const capacity_bytes)
    -> std::pmr::memory_resource* {
    return backing_ && backing_->can_allocate(capacity_bytes, ioj::memory::Backing::alignment)
             ? backing_.get()
             : nullptr;
}

auto USandboxEditorGameMemorySubsystem::backing_data() const noexcept -> std::byte* {
    return static_cast<bool>(backing_) ? backing_->data() : nullptr;
}
