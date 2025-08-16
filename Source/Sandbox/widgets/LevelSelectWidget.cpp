#include "Sandbox/widgets/LevelSelectWidget.h"

bool ULevelSelectWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    if (IsDesignTime()) {
        return true;
    }

    Button_Level1->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_1);
    Button_Level2->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_2);
    Button_Level3->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_3);
    Button_Level4->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_4);
    Button_Level5->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_5);
    Button_Level6->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_6);
    Button_Level7->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_7);
    Button_Level8->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_8);
    Button_Level9->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_9);
    Button_Level10->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_10);
    Button_Level11->OnClicked.AddDynamic(this, &ULevelSelectWidget::open_level_11);

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
void ULevelSelectWidget::open_level_7() {
    open_level_x(7);
}
void ULevelSelectWidget::open_level_8() {
    open_level_x(8);
}
void ULevelSelectWidget::open_level_9() {
    open_level_x(9);
}
void ULevelSelectWidget::open_level_10() {
    open_level_x(10);
}
void ULevelSelectWidget::open_level_11() {
    open_level_x(11);
}
void ULevelSelectWidget::open_level_x(int32 number) {
    auto const level_name_string{FString::Printf(TEXT("Level%d"), number)};
    auto const level_name{FName(*level_name_string)};

    UGameplayStatics::OpenLevel(GetWorld(), level_name);
}
