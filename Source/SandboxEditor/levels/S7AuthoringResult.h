#pragma once

#include <Containers/UnrealString.h>

#include <expected>

namespace ioj::editor {
template <typename T>
using S7AuthoringResult = std::expected<T, FString>;
}
