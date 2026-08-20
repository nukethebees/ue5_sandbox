#pragma once

#include "CoreMinimal.h"

#include "PayloadIndex.generated.h"

USTRUCT(BlueprintType)
struct FPayloadIndex {
    GENERATED_BODY()

    FPayloadIndex() = default;
    FPayloadIndex(uint8 tag, uint8 i)
        : type_tag(tag)
        , array_index(i) {}

    uint8 type_tag{0};
    uint8 array_index{0};
};

USTRUCT(BlueprintType)
struct FActorPayloadIndexes {
    GENERATED_BODY()

    FActorPayloadIndexes() = default;

    TArray<FPayloadIndex> indexes;
};
