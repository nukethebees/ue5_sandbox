#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace ml::ui::glow_lab {
auto make_widget() -> TSharedRef<SWidget>;
auto capture(FString const& output_directory) -> bool;
}
