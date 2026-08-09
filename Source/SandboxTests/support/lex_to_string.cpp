#include "lex_to_string.h"

#include "CoreMinimal.h"

auto LexToString(FVector3f const& vec) -> FString {
    return vec.ToCompactString();
}
