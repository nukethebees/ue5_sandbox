#pragma once

#include <codegen/generator.h>
#include <lispb/schema/type_graph.h>

#include <span>

namespace codegen::detail {

struct DeclarationEmission {
    Nodes header_prefix;
    Nodes header_generated_include;
    Nodes header;
    Nodes header_after;
    Nodes source;
    Nodes header_global;
    Nodes header_tail;
    Nodes source_global;
    Nodes source_tail;
    std::vector<Module> additional_modules;
    std::optional<std::string> tail_namespace;
    bool source_dependencies{true};
    bool format_generated{};
};

auto assemble_module(ModuleSettings const& settings, std::span<DeclarationEmission> emissions)
    -> std::vector<Module>;
auto lower_record(RecordSchema const& schema, TypeRegistry const& types) -> DeclarationEmission;
auto lower_union(UnionSchema const& schema, TypeRegistry const& types) -> DeclarationEmission;
auto lower_tagged_union(TaggedUnionSchema const& schema, TypeRegistry const& types)
    -> DeclarationEmission;
auto lower_static_table(StaticTableSchema const& schema, TypeRegistry const& types)
    -> DeclarationEmission;
auto lower_packed_value(PackedValueSchema const& schema,
                        TypeRegistry const& types,
                        lispb::schema::TypeGraph const& type_graph,
                        std::string const& module_name) -> DeclarationEmission;
auto lower_enum(EnumSchema const& schema,
                ModuleSettings const& settings,
                std::optional<std::string> const& helper_namespace,
                TypeRegistry const& types) -> DeclarationEmission;
auto lower_soa_declaration(SoaSchema const& schema,
                           NormalModuleSchema const& module,
                           TypeRegistry const& types,
                           lispb::schema::TypeGraph const& type_graph,
                           std::optional<std::string> const& allocator_prefix = std::nullopt)
    -> DeclarationEmission;
auto lower_homogeneous(HomogeneousLayoutSchema const& schema, TypeRegistry const& types)
    -> DeclarationEmission;
auto lower_facade(FacadeSchema const& schema, TypeRegistry const& types) -> DeclarationEmission;
auto lower_vector(VectorSoaSchema const& schema, SoaBackend backend, TypeRegistry const& types)
    -> DeclarationEmission;

auto lower_settings_module(SettingsModuleSchema const& module, TypeRegistry const& types) -> Module;

} // namespace codegen::detail
