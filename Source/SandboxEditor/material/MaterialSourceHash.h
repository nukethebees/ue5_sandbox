#pragma once

#include "CoreMinimal.h"

namespace material_synth {

SANDBOXEDITOR_API auto sha256(TConstArrayView<uint8> bytes) -> FString;

}
