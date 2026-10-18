#include <codegen/schema.h>

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/resolved_cpp_type_use.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

auto resolve_type_use(TypeRefId const reference_id, TypeRegistry const& types)
    -> ResolvedCppTypeUse {
    auto const& references{types.declarations.type_refs};
    auto const reference{references.index(reference_id)};
    auto& cpp_types{types.cpp_types};
    auto const name{std::string{cpp_types.strings().view(references.name[reference])}};
    CppTypeId result;
    if (name.starts_with('@')) {
        auto const key{std::string_view{name}.substr(1)};
        auto const found{types.find(key)};
        if (!found.has_value()) {
            throw std::invalid_argument{"Unknown C++ type reference: " + name};
        }
        result = types.registered.cpp_type[types.registered.index(*found)];
    } else {
        constexpr std::array fixed_width_integers{"std::int8_t",
                                                  "std::uint8_t",
                                                  "std::int16_t",
                                                  "std::uint16_t",
                                                  "std::int32_t",
                                                  "std::uint32_t",
                                                  "std::int64_t",
                                                  "std::uint64_t"};
        if (std::ranges::find(fixed_width_integers, name) != fixed_width_integers.end()) {
            result = cpp_types.intern(name, {cpp_types.dependency(name, "cstdint")});
        } else if (name == "std::size_t" || name == "std::ptrdiff_t") {
            result = cpp_types.intern(name, {cpp_types.dependency(name, "cstddef")});
        } else {
            result = cpp_types.intern(name);
        }
    }

    auto const base{classify_physical_type_use(cpp_types, result)};
    auto spelling{std::string{cpp_types.spelling(result)}};
    if (references.nested[reference].has_value()) {
        spelling += "::";
        spelling += cpp_types.strings().view(*references.nested[reference]);
    }
    spelling += cpp_types.strings().view(references.suffix[reference]);
    result = cpp_types.derive(result, spelling);
    auto physical{classify_physical_type_use(cpp_types, result)};
    physical.names_semantic_type = !references.nested[reference].has_value() &&
                                   cpp_types.strings().view(physical.object_spelling) ==
                                       cpp_types.strings().view(base.object_spelling) &&
                                   base.form == PhysicalTypeForm::value;
    return {.cpp_type = result, .physical = physical};
}

auto resolve_type(TypeRefId const reference, TypeRegistry const& types) -> CppTypeId {
    return resolve_type_use(reference, types).cpp_type;
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

} // namespace lispb::codegen
