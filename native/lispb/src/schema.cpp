#include <codegen/schema.h>

#include <stdexcept>

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
