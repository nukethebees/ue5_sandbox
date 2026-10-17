#pragma once

#include "native/memory/backing.h"
#include "native/memory/config.h"

#include "EditorSubsystem.h"

#include "GameMemoryEditorSubsystem.generated.h"

UCLASS()
class SANDBOXEDITOR_API USandboxEditorGameMemorySubsystem : public UEditorSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    auto acquire_backing() -> std::optional<ioj::memory::BackingLease>;
    auto backing_address() const noexcept -> std::byte*;
  private:
    void handle_editor_pre_exit();
    void end_active_pie_if_leased();

    std::unique_ptr<ioj::memory::Backing> backing_{};
};
