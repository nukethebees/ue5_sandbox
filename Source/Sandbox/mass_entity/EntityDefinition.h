#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"

#include "EntityDefinition.generated.h"

USTRUCT()
struct FEntityDefinition {
    GENERATED_BODY()

    FEntityDefinition() = default;
    FEntityDefinition(FMassArchetypeHandle handle,
                      FMassArchetypeSharedFragmentValues shared_fragments)
        : handle(handle)
        , shared_fragments(shared_fragments) {}

    FMassArchetypeHandle handle{};
    FMassArchetypeSharedFragmentValues shared_fragments{};
};
