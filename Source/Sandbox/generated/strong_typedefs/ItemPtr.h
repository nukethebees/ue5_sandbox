#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for TScriptInterface<IInventoryItem>
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/interfaces/inventory/InventoryItem.h"

#include "ItemPtr.generated.h"

USTRUCT(BlueprintType)
struct FItemPtr {
    GENERATED_BODY()
  private:
    UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    TScriptInterface<IInventoryItem> value{};

  public:
    explicit FItemPtr(TScriptInterface<IInventoryItem> v = {}) : value(v) {}
    template <typename... Args>
        requires std::constructible_from<TScriptInterface<IInventoryItem>, Args...>
    explicit FItemPtr(Args&&... args) : value(std::forward<Args>(args)...) {}

    explicit operator TScriptInterface<IInventoryItem>() const { return value; }

    TScriptInterface<IInventoryItem> get_value() const { return value; }

    // Dereference operators
    auto* operator->() { return &value; }
    auto& operator*() { return *value; }
    auto* operator->() const { return &value; }
    auto& operator*() const { return *value; }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FItemPtr const& obj) {
        return GetTypeHash(obj.value);
    }

};
