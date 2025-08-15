#include "Sandbox/widgets/LevelSelectWidget.h"

bool ULevelSelectWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    Button_Level1->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_1);
    Button_Level2->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_2);
    Button_Level3->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_3);
    Button_Level4->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_4);
    Button_Level5->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_5);
    Button_Level6->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_6);

    return true;
}
void ULevelSelectWidget::open_level_1() {
    open_level_x(1);
}
void ULevelSelectWidget::open_level_2() {
    open_level_x(2);
}
void ULevelSelectWidget::open_level_3() {
    open_level_x(3);
}
void ULevelSelectWidget::open_level_4() {
    open_level_x(4);
}
void ULevelSelectWidget::open_level_5() {
    open_level_x(5);
}
void ULevelSelectWidget::open_level_6() {
    open_level_x(6);
}
void ULevelSelectWidget::open_level_x(int32 number) {
    auto const level_name_string{FString::Printf(TEXT("Level%d"), number)};
    auto const level_name{FName(*level_name_string)};

    UGameplayStatics::OpenLevel(GetWorld(), level_name);
}
