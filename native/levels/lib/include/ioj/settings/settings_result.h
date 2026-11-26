#pragma once

#include <expected>
#include <string>

namespace ioj {
using SettingsResult = std::expected<void, std::string>;
}
