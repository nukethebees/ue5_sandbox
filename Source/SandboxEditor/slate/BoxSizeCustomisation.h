#pragma once

#include "SandboxEditor/slate/BoxSizeEditMode.h"

#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SVectorInputBox.h"

template <typename OptionType>
class SComboBox;
class SWidget;
class SBox;
class IPropertyHandle;

namespace ml {
class SBoxSizePropInput;
}

class FBoxSizeCustomisation : public IPropertyTypeCustomization {
  public:
    using ThisClass = FBoxSizeCustomisation;
    using Super = IPropertyTypeCustomization;

    using value_type = double;

    void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                         FDetailWidgetRow& HeaderRow,
                         IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
    void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
                           IDetailChildrenBuilder& StructBuilder,
                           IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

    static auto MakeInstance() -> TSharedRef<IPropertyTypeCustomization>;
  private:
    TSharedPtr<IPropertyHandle> property_handle;

    TSharedPtr<IPropertyHandle> box_size_handle{nullptr};
    TSharedPtr<IPropertyHandle> x_handle{nullptr};
    TSharedPtr<IPropertyHandle> y_handle{nullptr};
    TSharedPtr<IPropertyHandle> z_handle{nullptr};

    TSharedPtr<ml::SBoxSizePropInput> box_size_widget{nullptr};
};
