#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace material_synth {

using TextureResolveFunction = std::optional<std::string> (*)(void const*, std::string_view);

}
