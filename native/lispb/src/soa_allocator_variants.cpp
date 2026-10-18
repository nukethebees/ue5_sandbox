#include <codegen/schema/soa_allocator_variants.h>

#include <codegen/schema/declarations.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/soa_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {
namespace {

auto is_identifier_prefix(std::string_view const prefix) -> bool {
    return !prefix.empty() && std::isalpha(static_cast<unsigned char>(prefix.front())) &&
           std::ranges::all_of(prefix, [](unsigned char const character) {
               return std::isalnum(character) || character == '_';
           });
}

auto view_name(Declarations& declarations, DeclarationHandle schema, bool const is_const)
    -> IdentifierId {
    auto const authored{is_const
                            ? declarations.soas.const_view_name[declarations.soas.index(schema)]
                            : declarations.soas.view_name[declarations.soas.index(schema)]};
    if (authored) {
        return *authored;
    }
    return declarations.strings().intern(
        std::string{
            declarations.strings().view(declarations.soas.name[declarations.soas.index(schema)])} +
        (is_const ? "ConstView" : "View"));
}

auto prefixed(Declarations& declarations, IdentifierId const prefix, IdentifierId const name)
    -> IdentifierId {
    auto& strings{declarations.strings()};
    return strings.intern(std::string{strings.view(prefix)} + std::string{strings.view(name)});
}

void add_soa_names(Declarations& declarations,
                   std::set<IdentifierId>& names,
                   DeclarationHandle schema) {
    if (declarations.soas.emits_vector_storage(schema)) {
        names.insert(declarations.soas.name[declarations.soas.index(schema)]);
        names.insert(view_name(declarations, schema, false));
        names.insert(view_name(declarations, schema, true));
    }
    if (declarations.soas.single_allocation[declarations.soas.index(schema)]) {
        names.insert(declarations.strings().intern(declarations.strings().view(
            *declarations.soas.single_allocation[declarations.soas.index(schema)])));
    }
    if (declarations.soas.fixed[declarations.soas.index(schema)]) {
        auto const fixed{*declarations.soas.fixed[declarations.soas.index(schema)]};
        names.insert(declarations.fixed_soas.storage_name[declarations.fixed_soas.index(fixed)]);
        auto const containers{declarations.items(
            declarations.fixed_soas.containers[declarations.fixed_soas.index(fixed)])};
        names.insert(containers.begin(), containers.end());
    }
}
}

void validate_soa_allocator_variants(Declarations& declarations,
                                     SoaBackend const backend,
                                     std::span<DeclarationHandle const> const schemas,
                                     SchemaRange<SoaAllocatorVariantId> const variants) {
    if (variants.empty()) {
        return;
    }
    if (backend == SoaBackend::standard_library) {
        throw std::invalid_argument{"TArray allocator variants require the Unreal backend"};
    }

    std::set<IdentifierId> schema_names;
    std::set<IdentifierId> generated_names;
    for (auto const& schema : schemas) {
        schema_names.insert(declarations.soas.name[declarations.soas.index(schema)]);
        add_soa_names(declarations, generated_names, schema);
    }
    for (auto const variant_id : declarations.items(variants)) {
        auto const variant{variant_id};
        if (!is_identifier_prefix(declarations.strings().view(
                declarations.soa_allocator_variants
                    .prefix[declarations.soa_allocator_variants.index(variant)]))) {
            throw std::invalid_argument{
                "Invalid SoA allocator variant prefix: " +
                std::string{declarations.strings().view(
                    declarations.soa_allocator_variants
                        .prefix[declarations.soa_allocator_variants.index(variant)])}};
        }
        for (auto const& schema : schemas) {
            if (!declarations.soas.emits_vector_storage(schema)) {
                continue;
            }
            if (declarations.soas.fixed[declarations.soas.index(schema)] ||
                declarations.soas.field_mask_name[declarations.soas.index(schema)] ||
                declarations.soas.field_enum_name[declarations.soas.index(schema)] ||
                declarations.soas.equivalent_type[declarations.soas.index(schema)] ||
                !declarations.soas.functions[declarations.soas.index(schema)].empty() ||
                !declarations.soas.mutable_view_functions[declarations.soas.index(schema)]
                     .empty() ||
                !declarations.soas.const_view_functions[declarations.soas.index(schema)].empty() ||
                !declarations.soas.using_declarations[declarations.soas.index(schema)].empty()) {
                throw std::invalid_argument{"SoA allocator variants require plain dynamic schemas"};
            }
            for (auto const member_id :
                 declarations.items(declarations.soas.members[declarations.soas.index(schema)])) {
                auto const member{member_id};
                if (declarations.soa_members.kind[declarations.soa_members.index(member)] ==
                        SoaMemberKind::nested &&
                    (!declarations.soa_members
                          .nested_schema[declarations.soa_members.index(member)] ||
                     !schema_names.contains(
                         *declarations.soa_members
                              .nested_schema[declarations.soa_members.index(member)]))) {
                    throw std::invalid_argument{
                        "SoA allocator variants require generated nested schemas"};
                }
            }
            auto const names{std::array{
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         declarations.soas.name[declarations.soas.index(schema)]),
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         view_name(declarations, schema, false)),
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         view_name(declarations, schema, true)),
            }};
            for (auto const name : names) {
                if (!generated_names.insert(name).second) {
                    throw std::invalid_argument{"Duplicate SoA allocator variant type: " +
                                                std::string{declarations.strings().view(name)}};
                }
            }
        }
    }
}

auto expand_soa_allocator_variants(Declarations& declarations,
                                   std::span<DeclarationHandle const> const schemas,
                                   SchemaRange<SoaAllocatorVariantId> const variants)
    -> std::vector<DeclarationHandle> {
    std::vector<DeclarationHandle> expanded{schemas.begin(), schemas.end()};
    for (auto const variant_id : declarations.items(variants)) {
        auto const variant{variant_id};
        for (auto const& schema : schemas) {
            if (!declarations.soas.emits_vector_storage(schema)) {
                continue;
            }
            auto const copy{declarations.soas.clone(schema)};
            declarations.soas.name[declarations.soas.index(copy)] =
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         declarations.soas.name[declarations.soas.index(schema)]);
            declarations.soas.view_name[declarations.soas.index(copy)] =
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         view_name(declarations, schema, false));
            declarations.soas.const_view_name[declarations.soas.index(copy)] =
                prefixed(declarations,
                         declarations.soa_allocator_variants
                             .prefix[declarations.soa_allocator_variants.index(variant)],
                         view_name(declarations, schema, true));
            declarations.soas.storage[declarations.soas.index(copy)] = SoaStorage::vector;
            declarations.soas.single_allocation[declarations.soas.index(copy)].reset();
            declarations.soas.array_allocator[declarations.soas.index(copy)] =
                declarations.soa_allocator_variants
                    .allocator[declarations.soa_allocator_variants.index(variant)];
            expanded.push_back(copy);
        }
    }
    return expanded;
}

} // namespace lispb::codegen
