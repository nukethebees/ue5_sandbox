#include "TestVolumeDetailsCustomisation.h"

#include "BoxSizeAttrPropInput.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#include "Sandbox/misc/learning/TestVolume.h"

#include <Components/BoxComponent.h>
#include <DetailCategoryBuilder.h>
#include <DetailLayoutBuilder.h>
#include <DetailWidgetRow.h>
#include <Widgets/Text/STextBlock.h>

#define LOCTEXT_NAMESPACE "TestVolumeDetailsCustomisation"

auto FTestVolumeDetailsCustomisation::MakeInstance() -> TSharedRef<IDetailCustomization> {
    return MakeShared<FTestVolumeDetailsCustomisation>();
}

void FTestVolumeDetailsCustomisation::CustomizeDetails(IDetailLayoutBuilder& detail_builder) {
    get_actor(detail_builder);
    build_custom_rows(detail_builder);
}

void FTestVolumeDetailsCustomisation::get_actor(IDetailLayoutBuilder& detail_builder) {
    TArray<TWeakObjectPtr<UObject>> objects;
    detail_builder.GetObjectsBeingCustomized(objects);

    if (objects.Num() != 1) {
        selected_actor.Reset();
        return;
    }

    selected_actor = Cast<ATestVolume>(objects[0].Get());
}
void FTestVolumeDetailsCustomisation::build_custom_rows(IDetailLayoutBuilder& detail_builder) {
    auto& category{detail_builder.EditCategory(TEXT("Volume"), FText::GetEmpty())};
    // clang-format off
    category
        .AddCustomRow(INVTEXT("BoxSize"))
        .WholeRowContent()
        [
            SNew(ml::SBoxSizeAttrPropInput)
            .value_Lambda([this]() -> TOptional<FVector> {
                auto* box{get_box()};
                if (!box) {
                    return {};
                }

                return box->GetUnscaledBoxExtent();
            })
            .on_value_changed_Lambda([this](FVector const& value) {
                auto* box{get_box()};
                if (box) {
                    box->SetBoxExtent(value);
                }
            })
        ]
    ;
    // clang-format on
}
auto FTestVolumeDetailsCustomisation::get_box() -> UBoxComponent* {
    if (!selected_actor.IsValid()) {
        return {};
    }
    return selected_actor->GetComponentByClass<UBoxComponent>();
}

#undef LOCTEXT_NAMESPACE
