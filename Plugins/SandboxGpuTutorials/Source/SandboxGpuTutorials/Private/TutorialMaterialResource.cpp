#include "TutorialMaterialResource.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SlateMaterialBrush.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxGpuTutorialMaterial, Log, All);

FTutorialMaterialResource::~FTutorialMaterialResource() = default;

auto FTutorialMaterialResource::load(TCHAR const* const object_path,
                                     FVector2D const image_size,
                                     bool const create_dynamic_instance) -> bool {
    auto* const material{LoadObject<UMaterialInterface>(nullptr, object_path)};
    if (material == nullptr) {
        UE_LOG(LogSandboxGpuTutorialMaterial,
               Error,
               TEXT("Failed to load tutorial material '%s'."),
               object_path);
        return false;
    }

    material_.Reset(material);
    auto* brush_material{material};
    if (create_dynamic_instance) {
        auto* const dynamic_material{
            UMaterialInstanceDynamic::Create(material, GetTransientPackage())};
        if (dynamic_material == nullptr) {
            UE_LOG(LogSandboxGpuTutorialMaterial,
                   Error,
                   TEXT("Failed to create a dynamic instance of '%s'."),
                   object_path);
            material_.Reset();
            return false;
        }

        dynamic_material_.Reset(dynamic_material);
        brush_material = dynamic_material;
    }

    brush_ = MakeUnique<FSlateMaterialBrush>(*brush_material, image_size);
    return true;
}

auto FTutorialMaterialResource::get_brush() const -> FSlateBrush const* {
    return brush_.Get();
}

auto FTutorialMaterialResource::get_dynamic_material() const -> UMaterialInstanceDynamic* {
    return dynamic_material_.Get();
}
