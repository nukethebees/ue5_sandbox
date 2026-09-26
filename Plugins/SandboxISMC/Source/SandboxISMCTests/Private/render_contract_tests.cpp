#include "PSOPrecache.h"
#include "SandboxISMCComponent.h"

#include "AssetCompilingManager.h"
#include "Components/MapTestSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "StaticMeshResources.h"
#include "VertexFactory.h"
#include <CQTest.h>

namespace SandboxISMCContractTests {
class MeshCollector final : public FMeshElementCollector {
  public:
    MeshCollector(ERHIFeatureLevel::Type feature_level,
                  FSceneRenderingBulkObjectAllocator& allocator,
                  FRHICommandList& command_list)
        : FMeshElementCollector{feature_level, allocator} {
        // SandboxISMC supplies persistent geometry and only needs the collector's one-frame
        // resources.
        RHICmdList = &command_list;
    }
    using FMeshElementCollector::AddViewMeshArrays;
    using FMeshElementCollector::Finish;
    using FMeshElementCollector::SetPrimitive;
};
}

TEST_CLASS(SandboxISMCRenderContracts, "SandboxISMC.RenderTests")
{
    TUniquePtr<FMapTestSpawner> spawner;

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner.Reset(); }

    TEST_METHOD(ResolvedMaterialsSectionShadowsAndPrecacheMatchRealMeshBatches)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            auto* source{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
            if (!TestRunner->TestNotNull(TEXT("Cube loads"), source)) {
                return;
            }
            auto* mesh{DuplicateObject<UStaticMesh>(source, GetTransientPackage())};
            FAssetCompilingManager::Get().FinishAllCompilation();
            auto& lod{mesh->GetRenderData()->LODResources[0]};
            auto section{lod.Sections[0]};
            lod.Sections.SetNum(1);
            lod.Sections[0].bCastShadow = false;
            section.bCastShadow = true;
            lod.Sections.Add(section);

            auto* actor{world.SpawnActor<AActor>()};
            auto* component{NewObject<USandboxISMCComponent>(actor)};
            actor->SetRootComponent(component);
            actor->AddInstanceComponent(component);
            component->set_static_mesh(*mesh);
            auto* unsupported{NewObject<UMaterial>(component)};
            unsupported->MaterialDomain = MD_UI;
            unsupported->BlendMode = BLEND_Translucent;
            unsupported->bDisableDepthTest = true;
            unsupported->bAutomaticallySetUsageInEditor = false;
            component->SetMaterial(0, unsupported);
            component->SetCastShadow(true);
            component->set_instances(1, ESandboxISMCParallelism::Sequential, [](auto& chunk) {
                chunk.set_transform(
                    0, FVector3f::ZeroVector, FQuat4f::Identity, FVector3f::OneVector);
            });
            component->RegisterComponent();
            world.SendAllEndOfFrameUpdates();

            FPSOPrecacheParams base_params;
            component->SetupPrecachePSOParams(base_params);
            FMaterialInterfacePSOPrecacheParamsList precache;
            component->CollectPSOPrecacheData(base_params, precache);
            if (!TestRunner->TestEqual(
                    TEXT("Both section shadow states are precached"), precache.Num(), 2)) {
                return;
            }
            auto* fallback{UMaterial::GetDefaultMaterial(MD_Surface)};
            TestRunner->TestTrue(TEXT("Precache resolves the unsupported material"),
                                 precache[0].MaterialInterface == fallback);
            TestRunner->TestFalse(TEXT("Disabled section shadows are excluded from precaching"),
                                  precache[0].PSOPrecacheParams.bCastShadow);
            TestRunner->TestTrue(TEXT("Enabled section shadows are precached"),
                                 precache[1].PSOPrecacheParams.bCastShadow);
            auto* declaration{precache[0].VertexFactoryDataList[0].CustomDefaultVertexDeclaration};
            auto* proxy{component->GetSceneProxy()};
            if (!TestRunner->TestNotNull(TEXT("The component has a real scene proxy"), proxy)) {
                return;
            }

            auto const fallback_relevance{
                fallback->GetRelevance_Concurrent(world.Scene->GetShaderPlatform())};
            auto const feature_level{world.GetFeatureLevel()};
            auto* scene{world.Scene};
            ENQUEUE_RENDER_COMMAND(CheckSandboxISMCBatches)(
                [this, proxy, declaration, fallback, fallback_relevance, feature_level, scene](
                    FRHICommandListImmediate& command_list) {
                    FSceneViewFamilyContext family{FSceneViewFamily::ConstructionValues{
                        nullptr,
                        scene,
                        FEngineShowFlags{ESFIM_Game}}.SetTime(FGameTime::CreateUndilated(0.0,
                                                                                         0.0f))};
                    FSceneViewInitOptions options;
                    options.ViewFamily = &family;
                    options.SetViewRectangle(FIntRect{0, 0, 64, 64});
                    FSceneView view{options};
                    auto const relevance{proxy->GetViewRelevance(&view)};
                    TestRunner->TestEqual(TEXT("Draw relevance uses the resolved opaque material"),
                                          relevance.bOpaque != 0,
                                          fallback_relevance.bOpaque != 0);
                    TestRunner->TestEqual(TEXT("Occlusion uses the resolved depth-test state"),
                                          proxy->CanBeOccluded(),
                                          !fallback_relevance.bDisableDepthTest);

                    FSceneRenderingBulkObjectAllocator allocator;
                    SandboxISMCContractTests::MeshCollector collector{
                        feature_level, allocator, command_list};
                    TArray<FMeshBatchAndRelevance, SceneRenderingAllocator> meshes[2];
                    FSimpleElementCollector simple;
                    collector.AddViewMeshArrays(&view, &meshes[0], &simple, nullptr);
                    collector.AddViewMeshArrays(&view, &meshes[1], &simple, nullptr);
                    collector.SetPrimitive(proxy, FHitProxyId{});
                    TArray<FSceneView const*> views{&view, &view};
                    proxy->GetDynamicMeshElements(views, family, 3, collector);
                    if (TestRunner->TestEqual(
                            TEXT("LOD0 emits both sections"), meshes[0].Num(), 2) &&
                        TestRunner->TestEqual(
                            TEXT("Both views receive the sections"), meshes[1].Num(), 2)) {
                        auto const& first{*meshes[0][0].Mesh};
                        auto const& second{*meshes[0][1].Mesh};
                        TestRunner->TestFalse(
                            TEXT("The emitted batch honours disabled section shadows"),
                            first.CastShadow);
                        TestRunner->TestTrue(TEXT("The other emitted section still casts shadows"),
                                             second.CastShadow);
                        TestRunner->TestTrue(TEXT("The emitted batch draws the fallback"),
                                             first.MaterialRenderProxy ==
                                                 fallback->GetRenderProxy());
                        TestRunner->TestTrue(
                            TEXT("Sections and views share one primitive uniform resource"),
                            first.Elements[0].PrimitiveUniformBufferResource ==
                                    second.Elements[0].PrimitiveUniformBufferResource &&
                                first.Elements[0].PrimitiveUniformBufferResource ==
                                    meshes[1][0].Mesh->Elements[0].PrimitiveUniformBufferResource);
                        FVertexDeclarationElementList actual_elements;
                        FVertexDeclarationElementList precache_elements;
                        first.VertexFactory->GetDeclaration(EVertexInputStreamType::Default)
                            ->GetInitializer(actual_elements);
                        declaration->GetInitializer(precache_elements);
                        TestRunner->TestTrue(
                            TEXT("Precache matches the actual instanced vertex declaration"),
                            actual_elements == precache_elements);
                        int32 instance_rows{0};
                        for (auto const& element : actual_elements) {
                            if (element.AttributeIndex >= 8 && element.AttributeIndex <= 11) {
                                ++instance_rows;
                                TestRunner->TestTrue(
                                    TEXT("Instance rows use the 64-byte instanced float4 stream"),
                                    element.Stride == 64 && element.Type == VET_Float4 &&
                                        element.bUseInstanceIndex);
                            }
                        }
                        TestRunner->TestEqual(
                            TEXT("All four instance rows are present"), instance_rows, 4);
                    }
                    collector.Finish();
                });
            FlushRenderingCommands();
        });
    }

    TEST_METHOD(RejectsUnavailableOrStreamableLOD0BeforeCreatingAProxy)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            auto* actor{world.SpawnActor<AActor>()};
            auto* component{NewObject<USandboxISMCComponent>(actor)};
            actor->SetRootComponent(component);
            actor->AddInstanceComponent(component);
            component->set_static_mesh(*NewObject<UStaticMesh>(component));
            component->set_instances(1, ESandboxISMCParallelism::Sequential, [](auto& chunk) {
                chunk.set_transform(
                    0, FVector3f::ZeroVector, FQuat4f::Identity, FVector3f::OneVector);
            });
            TestRunner->AddExpectedError(
                TEXT("requires initialized, non-empty, inlined, non-optional LOD0"),
                EAutomationExpectedErrorFlags::Contains,
                1);
            component->RegisterComponent();
            TestRunner->TestNull(TEXT("An empty mesh has no scene proxy"),
                                 component->GetSceneProxy());

            auto* source{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
            if (!TestRunner->TestNotNull(TEXT("Cube loads"), source)) {
                return;
            }
            auto* mesh{DuplicateObject<UStaticMesh>(source, GetTransientPackage())};
            FAssetCompilingManager::Get().FinishAllCompilation();
            mesh->GetRenderData()->LODResources[0].bBuffersInlined = false;
            component->set_static_mesh(*mesh);
            component->set_instances(1, ESandboxISMCParallelism::Sequential, [](auto& chunk) {
                chunk.set_transform(
                    0, FVector3f::ZeroVector, FQuat4f::Identity, FVector3f::OneVector);
            });
            TestRunner->AddExpectedError(
                TEXT("requires initialized, non-empty, inlined, non-optional LOD0"),
                EAutomationExpectedErrorFlags::Contains,
                1);
            world.SendAllEndOfFrameUpdates();
            TestRunner->TestNull(TEXT("Streamable LOD0 is rejected even while resident"),
                                 component->GetSceneProxy());
        });
    }
};
