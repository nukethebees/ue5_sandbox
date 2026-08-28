#pragma once

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Vector2D.h"
#include "SlateMaterialBrush.h"
#include "UObject/StrongObjectPtr.h"

struct FSlateBrush;

class FTutorialMaterialResource {
  public:
    ~FTutorialMaterialResource();

    auto load(TCHAR const* object_path, FVector2D image_size, bool create_dynamic_instance = false)
        -> bool;

    [[nodiscard]] auto get_brush() const -> FSlateBrush const*;
    [[nodiscard]] auto get_dynamic_material() const -> UMaterialInstanceDynamic*;
  private:
    TStrongObjectPtr<UMaterialInterface> material_;
    TStrongObjectPtr<UMaterialInstanceDynamic> dynamic_material_;
    TUniquePtr<FSlateMaterialBrush> brush_;
};
