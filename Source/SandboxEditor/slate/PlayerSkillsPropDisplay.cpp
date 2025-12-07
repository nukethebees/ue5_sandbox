#include "SandboxEditor/slate/PlayerSkillsPropDisplay.h"

#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "Sandbox/players/playable/data/generated/player_skills.h"

void FPlayerSkillsPropDisplay::CustomizeHeader(
        TSharedRef<IPropertyHandle> PropertyHandle, 
        FDetailWidgetRow& HeaderRow, 
        IPropertyTypeCustomizationUtils& CustomizationUtils) {
                                    
}
void FPlayerSkillsPropDisplay::CustomizeChildren(
        TSharedRef<IPropertyHandle> PropertyHandle, 
        IDetailChildrenBuilder& ChildBuilder, 
        IPropertyTypeCustomizationUtils& CustomizationUtils) {
	if (!PropertyHandle->IsValidHandle())
	{
	    return;
	}
	 
    void* raw_data{nullptr};
    if (auto result{PropertyHandle->GetValueData(raw_data)}; result != FPropertyAccess::Result::Success) {
        return;
    }
    auto* data{reinterpret_cast<FPlayerSkills*>(raw_data)};

	for (auto value : ml::EPlayerSkillName_values) {
        auto const& name{ml::get_display_string_view(value)};
        auto skill{data->get(value)};

	    ChildBuilder.AddCustomRow(FText::FromStringView(name))
	        .NameContent()
	        [
	            SNew(STextBlock)
	            .Text(FText::FromStringView(name))
	            .Font(IDetailLayoutBuilder::GetDetailFont())
	        ]
            .ValueContent()
            [
                SNew(STextBlock).Text(FText::AsNumber(skill.get()))
            ];
    }
}
    
TSharedRef<IPropertyTypeCustomization> FPlayerSkillsPropDisplay::MakeInstance() {
    return MakeShareable(new FPlayerSkillsPropDisplay);
}
