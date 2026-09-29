#include "jobserver/types.hpp"

namespace jobserver {
auto path_to_utf8(std::filesystem::path const& value) -> std::string {
    auto const utf8{value.u8string()};
    return {reinterpret_cast<char const*>(utf8.data()), utf8.size()};
}

}
