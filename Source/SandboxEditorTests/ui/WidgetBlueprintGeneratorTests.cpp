#include <SandboxEditor/ui/WidgetBlueprintGenerator.h>

#include "WidgetBlueprintGeneratorTestWidgets.h"

#include <AssetRegistry/AssetData.h>
#include <Blueprint/WidgetTree.h>
#include <Components/CanvasPanel.h>
#include <CQTest.h>
#include <ObjectTools.h>
#include <UObject/Package.h>
#include <WidgetBlueprint.h>

namespace {
FString const test_output_directory{TEXT("/Game/Automation/WidgetBlueprintGenerator")};

FWidgetBlueprintGenerationEntry& add_test_entry(UWidgetBlueprintGenerationDataAsset& generator,
                                                UClass& widget_class,
                                                FName const widget_name) {
    auto& entry{generator.widgets.AddDefaulted_GetRef()};
    entry.generate = true;
    entry.output_directory.Path = test_output_directory;
    entry.widget_class = &widget_class;
    entry.widget_name = widget_name;
    return entry;
}

bool delete_generated_widget(FWidgetBlueprintGenerationEntry& entry,
                             UWidgetBlueprint* const widget_blueprint) {
    entry.existing_widget = nullptr;
    if (!IsValid(widget_blueprint)) {
        return true;
    }

    TArray<FAssetData> assets_to_delete{FAssetData{widget_blueprint}};
    return ObjectTools::DeleteAssets(assets_to_delete, false) == assets_to_delete.Num();
}

UWidgetBlueprintGenerationDataAsset* create_generator() {
    return NewObject<UWidgetBlueprintGenerationDataAsset>(GetTransientPackage());
}
}

TEST_CLASS(WidgetBlueprintGenerator, "SandboxEditor.UnitTests")
{
    TEST_METHOD(DefaultRoot)
    {
        auto* const generator{create_generator()};
        auto& entry{add_test_entry(*generator,
                                   *UWidgetBlueprintGeneratorDefaultRootTestWidget::StaticClass(),
                                   TEXT("WBP_GeneratorDefaultRootTest"))};
        generator->generate_widgets();

        auto* const widget_blueprint{entry.existing_widget.Get()};
        TestRunner->TestFalse(TEXT("Successful generation clears the row"), entry.generate);
        if (TestRunner->TestNotNull(TEXT("Generated Widget Blueprint is recorded"),
                                    widget_blueprint)) {
            auto const root_widget{widget_blueprint->WidgetTree->RootWidget};
            if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a root"),
                                     IsValid(root_widget))) {
                TestRunner->TestTrue(TEXT("Default root is a CanvasPanel"),
                                     root_widget->IsA<UCanvasPanel>());
                TestRunner->TestEqual(TEXT("Default root retains its name"),
                                      root_widget->GetFName(),
                                      FName{TEXT("RootCanvas")});
            }
        }

        TestRunner->TestTrue(TEXT("Generated Widget Blueprint is deleted during cleanup"),
                             delete_generated_widget(entry, widget_blueprint));
    }

    TEST_METHOD(PanelRoot)
    {
        auto* const generator{create_generator()};
        auto& entry{add_test_entry(*generator,
                                   *UWidgetBlueprintGeneratorPanelRootTestWidget::StaticClass(),
                                   TEXT("WBP_GeneratorPanelRootTest"))};
        generator->generate_widgets();

        auto* const widget_blueprint{entry.existing_widget.Get()};
        TestRunner->TestFalse(TEXT("Successful generation clears the row"), entry.generate);
        if (TestRunner->TestNotNull(TEXT("Generated Widget Blueprint is recorded"),
                                    widget_blueprint)) {
            auto const root_widget{widget_blueprint->WidgetTree->RootWidget};
            if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a root"),
                                     IsValid(root_widget))) {
                TestRunner->TestEqual(TEXT("GeneratorRoot becomes the Widget Tree root"),
                                      root_widget->GetFName(),
                                      FName{TEXT("root_widget")});

                TArray<UWidget*> generated_widgets;
                widget_blueprint->WidgetTree->GetAllWidgets(generated_widgets);
                int32 root_widget_count{0};

                for (auto const& widget : generated_widgets) {
                    if (widget == root_widget) {
                        ++root_widget_count;
                    }
                }

                TestRunner->TestEqual(
                    TEXT("GeneratorRoot is constructed only once"), root_widget_count, 1);
            }
        }

        TestRunner->TestTrue(TEXT("Generated Widget Blueprint is deleted during cleanup"),
                             delete_generated_widget(entry, widget_blueprint));
    }

    TEST_METHOD(NonPanelRootFails)
    {
        auto* const generator{create_generator()};
        auto& entry{add_test_entry(*generator,
                                   *UWidgetBlueprintGeneratorNonPanelRootTestWidget::StaticClass(),
                                   TEXT("WBP_GeneratorNonPanelRootTest"))};
        generator->generate_widgets();

        TestRunner->TestTrue(TEXT("Invalid GeneratorRoot leaves the row selected"), entry.generate);
        TestRunner->TestTrue(TEXT("Invalid GeneratorRoot does not generate an asset"),
                             entry.existing_widget == nullptr);
    }

    TEST_METHOD(MultipleRootsFail)
    {
        auto* const generator{create_generator()};
        auto& entry{add_test_entry(*generator,
                                   *UWidgetBlueprintGeneratorMultipleRootTestWidget::StaticClass(),
                                   TEXT("WBP_GeneratorMultipleRootTest"))};
        generator->generate_widgets();

        TestRunner->TestTrue(TEXT("Multiple GeneratorRoot properties leave the row selected"),
                             entry.generate);
        TestRunner->TestTrue(TEXT("Multiple GeneratorRoot properties do not generate an asset"),
                             entry.existing_widget == nullptr);
    }
};
