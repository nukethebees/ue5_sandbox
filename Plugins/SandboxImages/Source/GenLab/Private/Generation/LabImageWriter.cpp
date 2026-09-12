#include "Generation/GeneratedImageAssetWriter.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxImagesGenLab, Log, All);

namespace SandboxImages::GenLab {
using namespace sandbox::image;

namespace {
struct FTextureImportSettings {
    bool srgb{false};
    TextureCompressionSettings compression{TC_Masks};
    TextureGroup texture_group{TEXTUREGROUP_Effects};
    TextureMipGenSettings mip_generation{TMGS_FromTextureGroup};
    TextureFilter filter{TF_Default};
    TextureAddress address_x{TA_Clamp};
    TextureAddress address_y{TA_Clamp};
};

auto import_settings_for(GenerationRequest const& request) -> FTextureImportSettings {
    auto settings{FTextureImportSettings{}};
    if (request.generator == GeneratorType::CurlNoiseFlow) {
        settings.compression = TC_VectorDisplacementmap;
        if (request.curl_noise_flow.tileable) {
            settings.address_x = TA_Wrap;
            settings.address_y = TA_Wrap;
        }
    } else if (request.post_process.output == ImagePostProcessParameters::Output::NormalMap) {
        settings.compression = TC_Normalmap;
        if (request.post_process.normal_wrap) {
            settings.address_x = TA_Wrap;
            settings.address_y = TA_Wrap;
        }
    } else if (request.post_process.output ==
               ImagePostProcessParameters::Output::SignedDistance) {
        settings.compression = TC_Grayscale;
        if (request.post_process.distance_wrap) {
            settings.address_x = TA_Wrap;
            settings.address_y = TA_Wrap;
        }
    } else if (request.generator == GeneratorType::Noise ||
               request.generator == GeneratorType::DomainWarpedNoise) {
        settings.compression = TC_Grayscale;
        auto const tileable{request.generator == GeneratorType::Noise
                                ? request.noise.tileable
                                : request.domain_warped_noise.tileable};
        if (tileable) {
            settings.address_x = TA_Wrap;
            settings.address_y = TA_Wrap;
        }
    } else if (request.generator == GeneratorType::CellularNoise) {
        settings.compression = TC_Grayscale;
        if (request.cellular_noise.tileable) {
            settings.address_x = TA_Wrap;
            settings.address_y = TA_Wrap;
        }
    }
    return settings;
}

auto ensure_output_directory(FString const& output_directory) -> bool {
    if (output_directory.IsEmpty()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Cannot resolve the generated image output directory."));
        return false;
    }

    auto& file_manager{IFileManager::Get()};
    if (!file_manager.MakeDirectory(*output_directory, true) &&
        !file_manager.DirectoryExists(*output_directory)) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Failed to create Lab image output directory: %s"),
               *output_directory);
        return false;
    }
    return true;
}

auto validate_output_name(FString const& output_name,
                          FString const& destination_content_path) -> bool {
    FText reason;
    auto const object_path{FString::Printf(
        TEXT("%s/%s.%s"), *destination_content_path, *output_name, *output_name)};
    if (output_name.IsEmpty() || !FPackageName::IsValidObjectPath(object_path, &reason)) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Invalid generated image output name '%s': %s"),
               *output_name,
               *reason.ToString());
        return false;
    }
    return true;
}

auto to_unreal_pixels(GeneratedImage const& image) -> TArray<FColor> {
    TArray<FColor> pixels;
    pixels.Reserve(static_cast<int32>(image.pixels.size()));
    for (auto const pixel : image.pixels) {
        pixels.Emplace(pixel.red, pixel.green, pixel.blue, pixel.alpha);
    }
    return pixels;
}

auto write_png(FString const& output_path, GeneratedImage const& image) -> bool {
    if (!image.is_valid()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Cannot write %s: %s"),
               *output_path,
               image.error.empty() ? TEXT("generated image buffer is invalid.")
                                   : UTF8_TO_TCHAR(image.error.c_str()));
        return false;
    }

    auto const pixels{to_unreal_pixels(image)};
    FImageView const image_view{pixels.GetData(), image.width, image.height};
    if (!FImageUtils::SaveImageByExtension(*output_path, image_view)) {
        UE_LOG(LogSandboxImagesGenLab, Error, TEXT("Failed to write PNG: %s"), *output_path);
        return false;
    }

    auto const file_size{IFileManager::Get().FileSize(*output_path)};
    UE_LOG(
        LogSandboxImagesGenLab, Display, TEXT("Wrote %s (%lld bytes)."), *output_path, file_size);
    return true;
}

auto import_texture(FString const& source_path,
                    GenerationRequest const& request,
                    FTextureImportSettings const& settings,
                    FString const& destination_content_path) -> bool {
    auto* task{NewObject<UAssetImportTask>()};
    task->AddToRoot();
    task->Filename = source_path;
    task->DestinationPath = destination_content_path;
    task->DestinationName = UTF8_TO_TCHAR(request.output_name.c_str());
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
        texture->SRGB = settings.srgb;
        texture->CompressionSettings = settings.compression;
        texture->LODGroup = settings.texture_group;
        texture->MipGenSettings = settings.mip_generation;
        texture->Filter = settings.filter;
        texture->AddressX = settings.address_x;
        texture->AddressY = settings.address_y;
        texture->PostEditChange();

        auto* const package{texture->GetOutermost()};
        auto const description{FString{UTF8_TO_TCHAR(describe_request(request).c_str())}};
        package->GetMetaData().SetValue(
            texture, TEXT("SandboxImages.GenLab.Generation"), *description);
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

    if (!success && imported_objects.IsEmpty()) {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("Failed to import generated texture: %s"),
               *source_path);
    }

    task->RemoveFromRoot();
    return success;
}
}

auto get_output_directory(FString const& destination_content_path) -> FString {
    return FPackageName::LongPackageNameToFilename(destination_content_path);
}

auto generate_and_import(GenerationRequest const& request,
                         FString const& destination_content_path) -> bool {
    auto const output_directory{get_output_directory(destination_content_path)};
    auto const output_name{FString{UTF8_TO_TCHAR(request.output_name.c_str())}};
    if (!ensure_output_directory(output_directory) ||
        !validate_output_name(output_name, destination_content_path)) {
        return false;
    }

    auto const image{sandbox::image::generate_image(request)};
    auto const output_path{FPaths::Combine(output_directory, output_name + TEXT(".png"))};
    if (!write_png(output_path, image)) {
        return false;
    }
    return import_texture(
        output_path, request, import_settings_for(request), destination_content_path);
}

auto regenerate_all() -> bool {
    bool success{true};
    for (auto const& request : sandbox::image::default_generation_requests()) {
        success &= generate_and_import(request, lab_content_path);
    }

    if (success) {
        UE_LOG(LogSandboxImagesGenLab,
               Display,
               TEXT("Regenerated all SandboxImages Lab images in %s."),
               *get_output_directory(lab_content_path));
    } else {
        UE_LOG(LogSandboxImagesGenLab,
               Error,
               TEXT("One or more SandboxImages Lab images failed to regenerate."));
    }
    return success;
}

}
