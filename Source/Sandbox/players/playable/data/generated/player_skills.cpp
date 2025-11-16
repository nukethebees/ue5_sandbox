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
                               
}
    
TSharedRef<IPropertyTypeCustomization> FPlayerSkillsPropDisplay::MakeInstance() {
    return MakeShareable(new FPlayerSkillsPropDisplay);
}
