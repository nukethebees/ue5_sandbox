#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include <CQTest.h>

namespace SandboxMesh {

TEST_CLASS(MeshAssetWriter, "SandboxMesh.UnitTests")
{
    TEST_METHOD(SavesAUniqueHexFrameAssetInTheIgnoredTestDirectory)
    {
        FName asset_name;
        FString relative_directory;
        FString filename;
        do {
            auto const unique_id{FGuid::NewGuid().ToString(EGuidFormats::Digits)};
            asset_name = FName{FString::Printf(TEXT("SM_HexFrameTest_%s"), *unique_id)};
            relative_directory = FString::Printf(TEXT("Tests/Temp/%s"), *unique_id);
            filename = get_generated_asset_filename(asset_name, relative_directory);
        } while (IFileManager::Get().DirectoryExists(*FPaths::GetPath(filename)));

        auto request{make_default_mesh_request(ESbxMeshShape::HexFrame)};
        request.asset_name = SandboxMesh::to_native(asset_name);

        auto* const static_mesh{write_generated_static_mesh_asset(generate_mesh(request),
                                                                  asset_name,
                                                                  describe_mesh_request(request),
                                                                  relative_directory)};
        if (!TestRunner->TestTrue(TEXT("Unique static mesh was created"), IsValid(static_mesh))) {
            return;
        }

        TestRunner->TestTrue(TEXT("Unique static mesh package exists"),
                             IFileManager::Get().FileExists(*filename));

        FSoftObjectPath const object_path{
            get_generated_asset_object_path(asset_name, relative_directory)};
        auto const asset_data{
            FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(object_path)};
        TestRunner->TestTrue(TEXT("Unique static mesh is registered"), asset_data.IsValid());

        auto* const loaded_mesh{LoadObject<UStaticMesh>(nullptr, *object_path.ToString())};
        if (!TestRunner->TestTrue(TEXT("Unique static mesh can be loaded by object path"),
                                  IsValid(loaded_mesh))) {
            return;
        }

        FVector const expected_extent{50.0, 25.0 * FMath::Sqrt(3.0), 10.0};
        TestRunner->TestTrue(TEXT("Loaded static mesh retains the generated bounds"),
                             loaded_mesh->GetBounds().BoxExtent.Equals(expected_extent, 0.01));
        TestRunner->TestEqual(TEXT("Loaded static mesh retains all material slots"),
                              loaded_mesh->GetStaticMaterials().Num(),
                              mesh_material_role_count);
    }
};

}
