#include "MaterialExpressionUSFLoader.h"

#include "AssetCompilingManager.h"
#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialShared.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/ArchiveUObject.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"

TEST_CLASS(IncludedFunctionCompilation, "USFLoader.IncludedFunctionCompilation")
{
    TEST_METHOD(CompilesTransientMaterialUsingIncludedFunction)
    {
        if (!TestRunner->TestFalse(TEXT("Material compilation test uses a real RHI"),
                                   GUsingNullRHI)) {
            return;
        }

        auto* const transient_package{GetTransientPackage()};
        auto const material_name{MakeUniqueObjectName(
            transient_package, UMaterial::StaticClass(), TEXT("USFLoaderCompileTest"))};
        auto* const material{NewObject<UMaterial>(transient_package, material_name, RF_Transient)};
        if (!TestRunner->TestNotNull(TEXT("Transient material is created"), material)) {
            return;
        }
        FArchiveUObject resource_initialization_archive;
        material->Serialize(resource_initialization_archive);
        material->SetShadingModel(MSM_Unlit);

        auto* const loader{
            Cast<UMaterialExpressionUSFLoader>(UMaterialEditingLibrary::CreateMaterialExpression(
                material, UMaterialExpressionUSFLoader::StaticClass()))};
        auto* const included_function_call{
            Cast<UMaterialExpressionCustom>(UMaterialEditingLibrary::CreateMaterialExpression(
                material, UMaterialExpressionCustom::StaticClass()))};
        if (!TestRunner->TestNotNull(TEXT("USF Loader expression is created"), loader) ||
            !TestRunner->TestNotNull(TEXT("Downstream custom expression is created"),
                                     included_function_call)) {
            return;
        }

        loader->path_prefix = TEXT("/Plugin/USFLoader");
        loader->usf_file_paths = {TEXT("TestDummy.usf")};

        included_function_call->Code = TEXT("return TestDummyFunction() + previous_block;");
        included_function_call->OutputType = CMOT_Float1;
        auto& previous_block{included_function_call->Inputs.AddDefaulted_GetRef()};
        previous_block.InputName = TEXT("previous_block");

        bool const expressions_connected{UMaterialEditingLibrary::ConnectMaterialExpressions(
            loader, TEXT(""), included_function_call, TEXT("previous_block"))};
        bool const output_connected{UMaterialEditingLibrary::ConnectMaterialProperty(
            included_function_call, TEXT(""), MP_EmissiveColor)};
        if (!TestRunner->TestTrue(TEXT("Loader is connected to the included function call"),
                                  expressions_connected) ||
            !TestRunner->TestTrue(TEXT("Included function call is connected to the material"),
                                  output_connected)) {
            return;
        }

        FAssetCompilingManager::Get().FinishAllCompilation();
        GShaderCompilingManager->FinishAllCompilation();
        auto const cold_compilation{
            FParse::Param(FCommandLine::Get(), TEXT("SandboxFreshShaderCompilation"))};
        if (cold_compilation &&
            !TestRunner->TestTrue(
                TEXT("Fresh compilation bypasses cached shader jobs"),
                !IsShaderJobCacheDDCEnabled() &&
                    IConsoleManager::Get()
                        .FindConsoleVariable(TEXT("r.ShaderCompiler.DebugDiscardCacheOutputs"))
                        ->GetBool())) {
            return;
        }
        auto const compiled_before{GShaderCompilerStats->GetTotalShadersCompiled()};

        material->PostEditChange();
        // UE 5.8 material edits invalidate shader maps without requesting their permutations.
        material->CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
        FAssetCompilingManager::Get().FinishAllCompilation();
        GShaderCompilingManager->FinishAllCompilation();

        auto const compiled_jobs{GShaderCompilerStats->GetTotalShadersCompiled() - compiled_before};
        TestRunner->AddInfo(
            FString::Printf(TEXT("Material shader compiler jobs executed: %u (fresh=%d)"),
                            compiled_jobs,
                            cold_compilation));
        if (cold_compilation) {
            TestRunner->TestTrue(TEXT("Fresh compilation executes shader compiler jobs"),
                                 compiled_jobs > 0);
        }

        auto const* const resource{material->GetMaterialResource(GMaxRHIShaderPlatform)};
        if (!TestRunner->TestNotNull(TEXT("Transient material resource is created"), resource)) {
            return;
        }
        auto const& compile_errors{resource->GetCompileErrors()};
        auto const result_description{
            FString::Printf(TEXT("Transient material compiles without shader errors: %s"),
                            *FString::Join(compile_errors, TEXT("; ")))};
        TestRunner->TestTrue(*result_description, compile_errors.IsEmpty());
        TestRunner->TestTrue(TEXT("Transient material has a complete shader map"),
                             resource->IsGameThreadShaderMapComplete());
    }
};
