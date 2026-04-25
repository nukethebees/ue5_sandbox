#include "Cursor.h"

#include "Components/SceneComponent.h"

ACursor::ACursor() {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
