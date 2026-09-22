#include <codegen/schema/soa_allocator_variants.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <stdexcept>

namespace codegen {
namespace {

auto is_identifier_prefix(std::string const& prefix) -> bool {
    return !prefix.empty() && std::isalpha(static_cast<unsigned char>(prefix.front())) &&
           std::ranges::all_of(prefix, [](unsigned char const character) {
               return std::isalnum(character) || character == '_';
           });
}

void add_soa_names(std::set<std::string>& names, SoaSchema const& schema) {
    names.insert(schema.name);
    names.insert(schema.view_name.value_or(schema.name + "View"));
    names.insert(schema.const_view_name.value_or(schema.name + "ConstView"));
    if (schema.single_allocation.has_value()) {
        names.insert(*schema.single_allocation);
    }
    for (auto const& variant : schema.single_allocation_variants) {
        names.insert(variant.name);
    }
    if (schema.fixed.has_value()) {
        names.insert(schema.fixed->storage_name);
        names.insert(schema.fixed->containers.begin(), schema.fixed->containers.end());
    }
}

} // namespace

void validate_soa_allocator_variants(SoaBackend const backend,
                                     std::span<SoaSchema const> const schemas,
                                     std::span<SoaAllocatorVariant const> const variants) {
    if (variants.empty()) {
        return;
    }
    if (backend == SoaBackend::standard_library) {
        throw std::invalid_argument{"TArray allocator variants require the Unreal backend"};
    }

    std::set<std::string> schema_names;
    std::set<std::string> generated_names;
    for (auto const& schema : schemas) {
        schema_names.insert(schema.name);
        add_soa_names(generated_names, schema);
    }
    for (auto const& variant : variants) {
        if (!is_identifier_prefix(variant.prefix)) {
            throw std::invalid_argument{"Invalid SoA allocator variant prefix: " + variant.prefix};
        }
        for (auto const& schema : schemas) {
            if (schema.fixed.has_value() || schema.field_mask_name.has_value() ||
                schema.field_enum_name.has_value() || schema.equivalent_type.has_value() ||
                !schema.functions.empty() || !schema.mutable_view_functions.empty() ||
                !schema.using_declarations.empty()) {
                throw std::invalid_argument{"SoA allocator variants require plain dynamic schemas"};
            }
            for (auto const& member : schema.members) {
                if (member.kind != SoaMemberKind::nested) {
                    continue;
                }
                if (!member.nested_schema.has_value() ||
                    !schema_names.contains(*member.nested_schema)) {
                    throw std::invalid_argument{
                        "SoA allocator variants require generated nested schemas"};
                }
            }

            auto const names{std::array{
                variant.prefix + schema.name,
                variant.prefix + schema.view_name.value_or(schema.name + "View"),
                variant.prefix + schema.const_view_name.value_or(schema.name + "ConstView"),
            }};
            for (auto const& name : names) {
                if (!generated_names.insert(name).second) {
                    throw std::invalid_argument{"Duplicate SoA allocator variant type: " + name};
                }
            }
        }
    }
}

auto expand_soa_allocator_variants(SoaBackend const backend,
                                   std::span<SoaSchema const> const schemas,
                                   std::span<SoaAllocatorVariant const> const variants)
    -> std::vector<SoaSchema> {
    validate_soa_allocator_variants(backend, schemas, variants);

    std::vector<SoaSchema> expanded{schemas.begin(), schemas.end()};
    for (auto const& variant : variants) {
        for (auto const& schema : schemas) {
            auto copy{schema};
            copy.name = variant.prefix + schema.name;
            copy.view_name = variant.prefix + schema.view_name.value_or(schema.name + "View");
            copy.const_view_name =
                variant.prefix + schema.const_view_name.value_or(schema.name + "ConstView");
            copy.single_allocation.reset();
            copy.single_allocation_variants.clear();
            copy.array_allocator = variant.allocator;
            for (auto& member : copy.members) {
                if (member.kind == SoaMemberKind::nested) {
                    member.type = TypeRef{variant.prefix + *member.nested_schema};
                    member.nested_schema = variant.prefix + *member.nested_schema;
                }
            }
            expanded.push_back(std::move(copy));
        }
    }
    return expanded;
}

} // namespace codegen
