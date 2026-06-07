#include "Cursor.h"

#include "Components/ArrowComponent.h"

ACursor::ACursor() {
    RootComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Root"));
}
