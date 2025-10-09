#include "SandboxEditor/slate/StrongTypedefPreview.h"

#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"

TSharedRef<IPropertyTypeCustomization> FStrongTypedefPreview::MakeInstance() {
    return MakeShareable(new FStrongTypedefPreview);
}

void FStrongTypedefPreview::CustomizeHeader(TSharedRef<IPropertyHandle> struct_property_handle,
                                            FDetailWidgetRow& HeaderRow,
                                            IPropertyTypeCustomizationUtils& customisation_utils) {}

void
    FStrongTypedefPreview::CustomizeChildren(TSharedRef<IPropertyHandle> struct_property_handle,
                                             IDetailChildrenBuilder& child_builder,
                                             IPropertyTypeCustomizationUtils& customisation_utils) {
    uint32 n_children;
    struct_property_handle->GetNumChildren(n_children);
    auto const name{struct_property_handle->GetProperty()->GetDisplayNameText()};

    for (uint32 i = 0; i < n_children; ++i) {
        child_builder.AddProperty(struct_property_handle->GetChildHandle(i).ToSharedRef())
            .DisplayName(name);
    }
}
