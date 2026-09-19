#include <codegen/schema.h>

#include <array>
#include <stdexcept>
#include <utility>

namespace codegen {

auto resolve_type(TypeRef const& reference, std::map<std::string, CppType> const& types)
    -> CppType {
    CppType result;
    if (reference.name.starts_with('@')) {
        auto const key{reference.name.substr(1)};
        auto const found{types.find(key)};
        if (found == types.end()) {
            throw std::invalid_argument{"Unknown C++ type reference: " + reference.name};
        }
        result = found->second;
    } else {
        result = CppType{reference.name};
    }
    if (reference.nested.has_value()) {
        result.spelling += "::" + *reference.nested;
    }
    result.spelling += reference.suffix;
    return result;
}

auto native_spelling(std::string const& spelling) -> std::string {
    constexpr std::array integer_types{
        std::pair{"int8", "std::int8_t"},
        std::pair{"uint8", "std::uint8_t"},
        std::pair{"int16", "std::int16_t"},
        std::pair{"uint16", "std::uint16_t"},
        std::pair{"int32", "std::int32_t"},
        std::pair{"uint32", "std::uint32_t"},
        std::pair{"int64", "std::int64_t"},
        std::pair{"uint64", "std::uint64_t"},
    };
    for (auto const& [source, destination] : integer_types) {
        if (spelling == source) {
            return destination;
        }
    }
    return spelling;
}

auto all_storage_operations() -> std::vector<StorageOperation> {
    return {
        StorageOperation::reset,
        StorageOperation::reserve,
        StorageOperation::add_uninitialised,
        StorageOperation::add_defaulted,
        StorageOperation::remove_at_swap,
        StorageOperation::set_num,
        StorageOperation::copy_element,
        StorageOperation::append_from,
    };
}

} // namespace codegen
