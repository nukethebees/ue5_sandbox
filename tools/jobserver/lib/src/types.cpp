#include "jobserver/types.hpp"

namespace jobserver {
auto path_from_utf8(std::string const& value) -> std::filesystem::path {
    auto const* const begin{reinterpret_cast<char8_t const*>(value.data())};
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

auto path_to_utf8(std::filesystem::path const& value) -> std::string {
    auto const utf8{value.u8string()};
    return {reinterpret_cast<char const*>(utf8.data()), utf8.size()};
}

}
