#pragma once

#include "declaration_emission.h"
#include "lowering_context.h"

#include <codegen/ast/indices.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <map>
#include <optional>
#include <span>
#include <vector>

namespace lispb::codegen::detail {

auto assemble_module(LoweringContext& context,
                     ModuleSettingsId settings,
                     std::span<DeclarationEmission> emissions) -> std::vector<ModuleId>;
auto lower_record(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_settings_schema(LoweringContext& context, DeclarationHandle schema)
    -> DeclarationEmission;
auto lower_union(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_tagged_union(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_static_table(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_packed_value(LoweringContext& context,
                        DeclarationHandle schema,
                        schema::TypeId semantic_type) -> DeclarationEmission;
auto lower_enum(LoweringContext& context,
                DeclarationHandle schema,
                ModuleSettingsId settings,
                std::optional<StringId> helper_namespace) -> DeclarationEmission;
auto lower_soa_declaration(LoweringContext& context,
                           DeclarationHandle schema,
                           ModuleHandle module,
                           std::map<IdentifierId, DeclarationHandle> const& expanded,
                           std::optional<SoaAllocatorVariantId> allocator_variant = std::nullopt)
    -> DeclarationEmission;
auto lower_homogeneous(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_facade(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission;
auto lower_vector(LoweringContext& context, DeclarationHandle vector, SoaBackend backend)
    -> DeclarationEmission;
auto lower_settings_module(LoweringContext& context, ModuleHandle module) -> ModuleId;

} // namespace lispb::codegen::detail
