#include <SandboxEditor/ui/WidgetBlueprintGenerator.h>

#include "WidgetBlueprintGeneratorTestWidgets.h"

#include <AssetRegistry/AssetData.h>
#include <Blueprint/WidgetTree.h>
#include <Components/CanvasPanel.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
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
        if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint is recorded and valid"),
                                 IsValid(widget_blueprint))) {
            auto* const widget_tree{widget_blueprint->WidgetTree.Get()};
            if (!TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a valid Widget Tree"),
                                      IsValid(widget_tree))) {
                TestRunner->TestTrue(TEXT("Generated Widget Blueprint is deleted during cleanup"),
                                     delete_generated_widget(entry, widget_blueprint));
                return;
            }

            auto* const root_widget{widget_tree->RootWidget.Get()};
            if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a root"),
                                     IsValid(root_widget))) {
                TestRunner->TestTrue(TEXT("Default root is a CanvasPanel"),
                                     root_widget->IsA<UCanvasPanel>());
                TestRunner->TestEqual(TEXT("Default root retains its name"),
                                      root_widget->GetFName(),
                                      FName{TEXT("RootCanvas")});
            }

            TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a generated class"),
                                 IsValid(widget_blueprint->GeneratedClass));
            TestRunner->TestTrue(TEXT("Generated Widget Blueprint finished compiled and valid"),
                                 widget_blueprint->Status == BS_UpToDate);
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
        if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint is recorded and valid"),
                                 IsValid(widget_blueprint))) {
            auto* const widget_tree{widget_blueprint->WidgetTree.Get()};
            if (!TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a valid Widget Tree"),
                                      IsValid(widget_tree))) {
                TestRunner->TestTrue(TEXT("Generated Widget Blueprint is deleted during cleanup"),
                                     delete_generated_widget(entry, widget_blueprint));
                return;
            }

            auto* const root_widget{widget_tree->RootWidget.Get()};
            if (TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a root"),
                                     IsValid(root_widget))) {
                TestRunner->TestEqual(TEXT("GeneratorRoot becomes the Widget Tree root"),
                                      root_widget->GetFName(),
                                      FName{TEXT("root_widget")});
                TestRunner->TestTrue(TEXT("GeneratorRoot has the expected panel type"),
                                     root_widget->IsA<UVerticalBox>());

                auto* const text_widget{widget_tree->FindWidget(TEXT("text_widget"))};
                TestRunner->TestTrue(TEXT("Generated text_widget exists"),
                                     IsValid(text_widget));

                TArray<UWidget*> generated_widgets;
                widget_tree->GetAllWidgets(generated_widgets);
                int32 root_widget_count{0};
                FName const root_widget_name{TEXT("root_widget")};

                for (auto const& widget : generated_widgets) {
                    if (IsValid(widget) && widget->GetFName() == root_widget_name) {
                        ++root_widget_count;
                    }
                }

                TestRunner->TestEqual(
                    TEXT("GeneratorRoot is constructed only once"), root_widget_count, 1);
            }

            TestRunner->TestTrue(TEXT("Generated Widget Blueprint has a generated class"),
                                 IsValid(widget_blueprint->GeneratedClass));
            TestRunner->TestTrue(TEXT("Generated Widget Blueprint finished compiled and valid"),
                                 widget_blueprint->Status == BS_UpToDate);
        }

        TestRunner->TestTrue(TEXT("Generated Widget Blueprint is deleted during cleanup"),
                             delete_generated_widget(entry, widget_blueprint));
    }

    TEST_METHOD(NonPanelRootFails)
    {
        Assert.ExpectError(TEXT("GeneratorRoot properties must derive from UPanelWidget."));
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
        Assert.ExpectError(TEXT("are both marked GeneratorRoot."));
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
