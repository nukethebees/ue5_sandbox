#include "Generation/LabImageWriter.h"

#include "Generation/ImageGenerators.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxImagesGenLab, Log, All);

namespace SandboxImages::GenLab {
namespace {
auto write_png(FString const& output_directory,
               TCHAR const* const filename,
               FGeneratedImage const& image) -> bool {
    if (!image.is_valid()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Cannot write %s: %s"),
               filename,
               image.error.IsEmpty() ? TEXT("generated image buffer is invalid.") : *image.error);
        return false;
    }

    auto const output_path{FPaths::Combine(output_directory, filename)};
    FImageView const image_view{image.pixels.GetData(), image.width, image.height};
    if (!FImageUtils::SaveImageByExtension(*output_path, image_view)) {
        UE_LOG(LogSandboxImagesGenLab, Error, TEXT("Failed to write PNG: %s"), *output_path);
        return false;
    }

    auto const file_size{IFileManager::Get().FileSize(*output_path)};
    UE_LOG(
        LogSandboxImagesGenLab, Display, TEXT("Wrote %s (%lld bytes)."), *output_path, file_size);
    return true;
}

auto import_texture(FString const& source_path, bool const is_mask) -> bool {
    auto* task{NewObject<UAssetImportTask>()};
    task->AddToRoot();
    task->Filename = source_path;
    task->DestinationPath = TEXT("/SandboxImages/Lab/Images");
    task->DestinationName = FPaths::GetBaseFilename(source_path);
    task->bAutomated = true;
    task->bReplaceExisting = true;
    task->bReplaceExistingSettings = true;
    task->bSave = false;
    task->bAsync = false;

    FAssetToolsModule::GetModule().Get().ImportAssetTasks({task});

    bool success{false};
    auto const& imported_objects{task->GetObjects()};
    for (auto* const imported_object : imported_objects) {
        auto* const texture{Cast<UTexture2D>(imported_object)};
        if (texture == nullptr) {
            continue;
        }

        texture->Modify();
        texture->SRGB = false;
        texture->CompressionSettings = is_mask ? TC_Masks : TC_Grayscale;
        texture->PostEditChange();

        auto* const package{texture->GetOutermost()};
        package->MarkPackageDirty();
        auto const package_path{FPackageName::LongPackageNameToFilename(
            package->GetName(), FPackageName::GetAssetPackageExtension())};
        FSavePackageArgs save_arguments{};
        save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
        success = UPackage::SavePackage(package, texture, *package_path, save_arguments);
        if (success) {
            UE_LOG(LogSandboxImagesGenLab,
                   Display,
                   TEXT("Imported texture asset %s."),
                   *texture->GetPathName());
        } else {
            UE_LOG(LogSandboxImagesGenLab,
                   Error,
                   TEXT("Failed to save imported texture asset: %s"),
                   *package_path);
        }
        break;
    }

    if (imported_objects.IsEmpty()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Failed to import generated texture: %s"),
               *source_path);
    }

    task->RemoveFromRoot();
    return success;
}
}

auto regenerate_all() -> bool {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxImages"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Cannot find the SandboxImages plugin output directory."));
        return false;
    }

    auto const output_directory{
        FPaths::Combine(plugin->GetContentDir(), TEXT("Lab"), TEXT("Images"))};
    auto& file_manager{IFileManager::Get()};
    if (!file_manager.MakeDirectory(*output_directory, true) &&
        !file_manager.DirectoryExists(*output_directory)) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Failed to create Lab image output directory: %s"),
               *output_directory);
        return false;
    }

    bool success{true};
    success &= write_png(output_directory,
                         TEXT("soft_radial_gradient.png"),
                         generate_radial_gradient(FRadialGradientParameters{}));
    success &= write_png(
        output_directory, TEXT("ring_mask.png"), generate_ring_mask(FRingMaskParameters{}));
    success &= write_png(
        output_directory, TEXT("starfield.png"), generate_starfield(FStarfieldParameters{}));
    success &=
        write_png(output_directory, TEXT("coherent_noise.png"), generate_noise(FNoiseParameters{}));
    success &= write_png(
        output_directory, TEXT("hex_grid_mask.png"), generate_hex_grid(FHexGridParameters{}));

    if (success) {
        success &= import_texture(
            FPaths::Combine(output_directory, TEXT("soft_radial_gradient.png")), true);
        success &= import_texture(FPaths::Combine(output_directory, TEXT("ring_mask.png")), true);
        success &= import_texture(FPaths::Combine(output_directory, TEXT("starfield.png")), true);
        success &=
            import_texture(FPaths::Combine(output_directory, TEXT("coherent_noise.png")), false);
        success &=
            import_texture(FPaths::Combine(output_directory, TEXT("hex_grid_mask.png")), true);
    }

    if (success) {
        UE_LOG(LogSandboxImagesGenLab,
               Display,
               TEXT("Regenerated all SandboxImages Lab images in %s."),
               *output_directory);
    } else {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("One or more SandboxImages Lab images failed to regenerate."));
    }
    return success;
}

}
