#include <codegen/schema.h>

#include "schema_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen {

auto integer_domain(IntegerScalarSchema const& scalar) -> IntegerDomainView {
    return {scalar.signedness,
            scalar.minimum_value,
            scalar.maximum_value,
            scalar.bit_width,
            scalar.named_codes};
}

auto integer_domain(ExternalIntegerSchema const& scalar) -> IntegerDomainView {
    return {scalar.signedness,
            scalar.minimum_value,
            scalar.maximum_value,
            scalar.bit_width,
            scalar.named_codes};
}

auto external_scalar_schema(TypeRegistry const& types, std::string const& spelling)
    -> ExternalScalarSchema const* {
    auto const normalized{native_spelling(spelling)};
    ExternalScalarSchema const* result{};
    for (auto const& [name, registered] : types) {
        if (std::holds_alternative<std::monostate>(registered.semantics) ||
            native_spelling(registered.cpp_type.spelling) != normalized) {
            continue;
        }
        if (result != nullptr && *result != registered.semantics) {
            throw std::invalid_argument{"Conflicting scalar descriptions for C++ type '" +
                                        spelling + "'"};
        }
        result = &registered.semantics;
    }
    return result;
}

auto resolve_type_use(TypeRef const& reference, TypeRegistry const& types) -> ResolvedCppTypeUse {
    CppType result;
    if (reference.name.starts_with('@')) {
        auto const key{reference.name.substr(1)};
        auto const found{types.find(key)};
        if (found == types.end()) {
            throw std::invalid_argument{"Unknown C++ type reference: " + reference.name};
        }
        result = found->second.cpp_type;
    } else {
        result = CppType{reference.name};
        constexpr std::array fixed_width_integers{"std::int8_t",
                                                  "std::uint8_t",
                                                  "std::int16_t",
                                                  "std::uint16_t",
                                                  "std::int32_t",
                                                  "std::uint32_t",
                                                  "std::int64_t",
                                                  "std::uint64_t"};
        if (std::ranges::find(fixed_width_integers, reference.name) != fixed_width_integers.end()) {
            result.dependencies.push_back({reference.name, "cstdint", {}});
        }
        if (reference.name == "std::size_t" || reference.name == "std::ptrdiff_t") {
            result.dependencies.push_back({reference.name, "cstddef", {}});
        }
    }
    auto const base{classify_physical_type_use(result.spelling)};
    if (reference.nested.has_value()) {
        result.spelling += "::" + *reference.nested;
    }
    result.spelling += reference.suffix;
    auto physical{classify_physical_type_use(result.spelling)};
    physical.names_semantic_type = !reference.nested.has_value() &&
                                   physical.object_spelling == base.object_spelling &&
                                   base.form == PhysicalTypeForm::value;
    return {.cpp_type = std::move(result), .physical = std::move(physical)};
}

auto resolve_type(TypeRef const& reference, TypeRegistry const& types) -> CppType {
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

} // namespace codegen

namespace codegen::detail {

auto output_path_key(std::filesystem::path const& path) -> std::string {
    auto result{path.lexically_normal().generic_string()};
#ifdef _WIN32
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return result;
}

} // namespace codegen::detail
