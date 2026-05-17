#pragma once

#include "SandboxEditor/slate/BoxSizeEditMode.h"

#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SVectorInputBox.h"

template <typename OptionType>
class SComboBox;
class SWidget;
class SBox;
class IPropertyHandle;

class FBoxSizeCustomisation : public IPropertyTypeCustomization {
  public:
    using ThisClass = FBoxSizeCustomisation;
    using Super = IPropertyTypeCustomization;

    using value_type = double;

    using ScalarInput = SNumericEntryBox<value_type>;
    using Vec2Input = SNumericVectorInputBox<value_type, UE::Math::TVector2<value_type>, 2>;
    using Vec3Input = SNumericVectorInputBox<value_type, UE::Math::TVector<value_type>, 3>;

    void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                         FDetailWidgetRow& HeaderRow,
                         IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
    void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
                           IDetailChildrenBuilder& StructBuilder,
                           IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

    static auto MakeInstance() -> TSharedRef<IPropertyTypeCustomization>;
  private:
    auto make_box_size_value_widget() -> TSharedRef<SWidget>;
    auto make_xyz_widget() -> TSharedRef<SWidget>;
    auto make_uniform_widget() -> TSharedRef<SWidget>;
    auto make_xy_and_z_widget() -> TSharedRef<SWidget>;

    auto read_prop(TSharedPtr<IPropertyHandle> handle) -> TOptional<value_type>;
    void write_prop(TSharedPtr<IPropertyHandle> handle, value_type value);

    TSharedPtr<IPropertyHandle> property_handle;

    // Edit mode
    TSharedPtr<SComboBox<FName>> edit_mode_combobox{nullptr};
    EBoxSizeEditMode edit_mode{EBoxSizeEditMode::uniform};
    TArray<FName> edit_options;

    // Value
    TSharedPtr<SBox> box_size_widget_container;

    TSharedPtr<IPropertyHandle> box_size_handle{nullptr};
    TSharedPtr<IPropertyHandle> x_handle{nullptr};
    TSharedPtr<IPropertyHandle> y_handle{nullptr};
    TSharedPtr<IPropertyHandle> z_handle{nullptr};
};
