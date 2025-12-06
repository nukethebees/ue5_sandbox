#include "SandboxEditor/slate/PlayerSkillsPropDisplay.h"

#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

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
}
    
TSharedRef<IPropertyTypeCustomization> FPlayerSkillsPropDisplay::MakeInstance() {
    return MakeShareable(new FPlayerSkillsPropDisplay);
}
