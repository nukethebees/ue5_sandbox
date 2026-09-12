#include <sandbox/core/static_string_view.h>

#include <cstddef>

auto reject_automatic_storage() -> std::size_t {
    constexpr char local_string[]{"automatic"};
    ml::StaticStringView const value{local_string};
    return value.size();
}
