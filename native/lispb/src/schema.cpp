#include <codegen/schema.h>

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/resolved_cpp_type_use.h>
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
        auto const spelling{native_spelling(name)};
        constexpr std::array fixed_width_integers{"std::int8_t",
                                                  "std::uint8_t",
                                                  "std::int16_t",
                                                  "std::uint16_t",
                                                  "std::int32_t",
                                                  "std::uint32_t",
                                                  "std::int64_t",
                                                  "std::uint64_t"};
        if (std::ranges::find(fixed_width_integers, spelling) != fixed_width_integers.end()) {
            result = cpp_types.intern(spelling, {cpp_types.dependency(spelling, "cstdint")});
        } else if (spelling == "std::size_t" || spelling == "std::ptrdiff_t") {
            result = cpp_types.intern(spelling, {cpp_types.dependency(spelling, "cstddef")});
        } else {
            result = cpp_types.intern(spelling);
        }
    }

    result = cpp_types.reference(result);
    auto const base{classify_physical_type_use(cpp_types, result)};
    std::string suffix;
    if (references.nested[reference].has_value()) {
        suffix += "::";
        suffix += cpp_types.strings().view(*references.nested[reference]);
    }
    suffix += cpp_types.strings().view(references.suffix[reference]);
    if (!suffix.empty()) {
        result = cpp_types.decorate(result, "", suffix);
    }
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

} // namespace lispb::codegen
