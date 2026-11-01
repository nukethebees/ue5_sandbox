#pragma once

#include <ioj/memory/bump_resource.h>

#include "EditorSubsystem.h"

#include "GameMemoryEditorSubsystem.generated.h"

UCLASS()
class SANDBOXEDITOR_API USandboxEditorGameMemorySubsystem : public UEditorSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    auto upstream_resource() -> ioj::memory::BumpResource&;
  private:
    void handle_editor_pre_exit();
    void end_active_pie();

    std::unique_ptr<ioj::memory::BumpResource> backing_{};
};
