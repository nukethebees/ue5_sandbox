#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for TScriptInterface<IInventoryItem>
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

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
