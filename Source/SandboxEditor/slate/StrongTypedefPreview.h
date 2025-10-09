#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class FDetailWidgetRow;
class IPropertyTypeCustomizationUtils;

// Used with the strong typedef system
// Change the display of the struct so it just shows the inner value
//
// e.g.
// [struct variable name] | [nothing]
//     "value" | [value]
// It would instead display
// [struct variable name] | [value]
class FStrongTypedefPreview : public IPropertyTypeCustomization {
  public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();

    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> struct_property_handle,
                                 FDetailWidgetRow& HeaderRow,
                                 IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> struct_property_handle,
                                   IDetailChildrenBuilder& child_builder,
                                   IPropertyTypeCustomizationUtils& CustomizationUtils) override;
};
