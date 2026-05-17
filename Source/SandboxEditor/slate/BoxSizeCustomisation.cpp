#include "BoxSizeCustomisation.h"

#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include "SandboxEditor/slate/BoxSizeEditMode.h"
#include "SandboxEditor/slate/BoxSizePropInput.h"

#include "Sandbox/utilities/BoxSize.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void FBoxSizeCustomisation::CustomizeHeader(TSharedRef<IPropertyHandle> prop_handle,
                                            FDetailWidgetRow& header_row,
                                            IPropertyTypeCustomizationUtils& customisation_utils) {
    // clang-format off
    header_row
        .WholeRowContent()
        [
            prop_handle->CreatePropertyNameWidget()
        ];
    // clang-format on
}

void
    FBoxSizeCustomisation::CustomizeChildren(TSharedRef<IPropertyHandle> prop_handle,
                                             IDetailChildrenBuilder& child_builder,
                                             IPropertyTypeCustomizationUtils& customisation_utils) {

    if (!prop_handle->IsValidHandle()) {
        return;
    }

    property_handle = prop_handle;
    box_size_handle = property_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBoxSize, box_size));
    x_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, X));
    y_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Y));
    z_handle = box_size_handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Z));

    // clang-format off
    child_builder.AddCustomRow(INVTEXT("Vector Edit Mode"))
        .WholeRowContent()
        [
            SAssignNew(box_size_widget, ml::SBoxSizePropInput)
            .x_handle(x_handle)
            .y_handle(y_handle)
            .z_handle(z_handle)
        ];

    // clang-format on
}

auto FBoxSizeCustomisation::MakeInstance() -> TSharedRef<IPropertyTypeCustomization> {
    return MakeShareable(new ThisClass);
}
