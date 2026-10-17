#pragma once

#include "native/memory/config.h"
#include "native/memory/retained_backing_resource.h"

#include "EditorSubsystem.h"

#include "GameMemoryEditorSubsystem.generated.h"

UCLASS()
class SANDBOXEDITOR_API USandboxEditorGameMemorySubsystem : public UEditorSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    auto upstream_resource(std::size_t capacity_bytes) -> std::pmr::memory_resource*;
    auto backing_data() const noexcept -> std::byte*;
  private:
    void handle_editor_pre_exit();
    void end_active_pie_if_memory_in_use();

    std::unique_ptr<ioj::memory::RetainedBackingResource> backing_{};
};
