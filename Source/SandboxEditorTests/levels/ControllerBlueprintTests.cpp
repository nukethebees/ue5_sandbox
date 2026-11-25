#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <CQTest.h>
#include <EdGraph/EdGraph.h>
#include <EdGraph/EdGraphNode.h>
#include <Engine/Blueprint.h>
#include <Engine/BlueprintGeneratedClass.h>
#include <Engine/InputDelegateBinding.h>
#include <Engine/SimpleConstructionScript.h>

TEST_CLASS(ControllerBlueprint, "SandboxEditor.Authoring")
{
    TEST_METHOD(ProductionControllerBlueprintHasNoInputGraph)
    {
        auto* const blueprint{LoadObject<UBlueprint>(
            nullptr,
            TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController.BP_SpaceGamePlayerController"))};
        if (!TestRunner->TestNotNull(TEXT("Production controller Blueprint"), blueprint)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Direct C++ controller parent"),
                             blueprint->ParentClass ==
                                 ioj::ASpaceGamePlayerController::StaticClass());
        TArray<UEdGraph*> graphs;
        blueprint->GetAllGraphs(graphs);
        for (auto const* const graph : graphs) {
            for (auto const& node_ptr : graph->Nodes) {
                auto const* const node{node_ptr.Get()};
                if (graph->GetFName() == TEXT("UserConstructionScript") &&
                    node->GetClass()->GetFName() == TEXT("K2Node_FunctionEntry")) {
                    continue;
                }
                TestRunner->AddError(
                    FString::Printf(TEXT("Unexpected controller graph node: %s/%s"),
                                    *graph->GetName(),
                                    *node->GetClass()->GetName()));
            }
        }
        TestRunner->TestTrue(TEXT("No Blueprint-added components"),
                             blueprint->SimpleConstructionScript == nullptr ||
                                 blueprint->SimpleConstructionScript->GetAllNodes().IsEmpty());
        auto const* const generated{Cast<UBlueprintGeneratedClass>(blueprint->GeneratedClass)};
        if (!TestRunner->TestNotNull(TEXT("Generated controller class"), generated)) {
            return;
        }

        for (auto const& binding_ptr : generated->DynamicBindingObjects) {
            auto const* const binding{binding_ptr.Get()};
            TestRunner->TestFalse(TEXT("No Blueprint input binding"),
                                  IsValid(binding) && binding->IsA<UInputDelegateBinding>());
        }
    }
};
