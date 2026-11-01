#include "GameMemoryEditorSubsystem.h"

#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

#include <Editor.h>
#include <Editor/EditorEngine.h>

void USandboxEditorGameMemorySubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    backing_ = std::make_unique<ioj::memory::BumpResource>();
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
    end_active_pie();
    backing_.reset();

    Super::Deinitialize();
}

void USandboxEditorGameMemorySubsystem::handle_editor_pre_exit() {
    end_active_pie();
}

void USandboxEditorGameMemorySubsystem::end_active_pie() {
    if (IsValid(GEditor) && GEditor->PlayWorld != nullptr) {
        GEditor->EndPlayMap();
    }
}

auto USandboxEditorGameMemorySubsystem::upstream_resource() -> ioj::memory::BumpResource& {
    return *backing_;
}
