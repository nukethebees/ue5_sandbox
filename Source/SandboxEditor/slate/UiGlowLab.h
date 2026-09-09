#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace ml::ui::glow_lab {
auto make_widget() -> TSharedRef<SWidget>;
auto generate_material() -> bool;
auto capture(FString const& output_directory) -> bool;
}
