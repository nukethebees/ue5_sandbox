#include <ioj/layout/lispb_adapter.hpp>

#include <codegen/schema.h>
#include <codegen/source_loader.h>
#include <codegen/validation.h>
#include <lispb/project.h>

#include <algorithm>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>

namespace ioj::layout {
namespace {

auto field_kind(codegen::PackedFieldKind const kind) -> PackedFieldKind {
    switch (kind) {
        case codegen::PackedFieldKind::unsigned_integer:
            return PackedFieldKind::unsigned_integer;
        case codegen::PackedFieldKind::enumeration:
            return PackedFieldKind::enumeration;
    }
    return PackedFieldKind::unsigned_integer;
}

auto qualified_name(codegen::ModuleSettings const& settings, std::string const& name)
    -> std::string {
    return settings.namespace_name.has_value() ? *settings.namespace_name + "::" + name : name;
}

void add_type_representation(std::vector<TypeRepresentation>& representations,
                             std::vector<Diagnostic>& diagnostics,
                             std::string spelling,
                             std::string represented_by) {
    auto const found{std::ranges::find(representations, spelling, &TypeRepresentation::spelling)};
    if (found == representations.end()) {
        representations.push_back(
            {.spelling = std::move(spelling), .represented_by = std::move(represented_by)});
        return;
    }
    if (found->represented_by != represented_by) {
        diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Conflicting schema-derived representations for type '" + spelling + "'."});
    }
}

void add_packed_module(CatalogLoadResult& result,
                       codegen::PackedValueModuleSchema const& module,
                       codegen::Manifest const& manifest) {
    for (auto const& value : module.values) {
        PackedLayout layout{
            .id = {.kind = SchemaKind::packed_value,
                   .module_name = module.settings.name,
                   .schema_name = value.name},
            .storage_type = codegen::native_spelling(
                codegen::resolve_type(value.storage_type, manifest.types).spelling),
            .fields = {},
            .invalid_raw_value = value.invalid_value,
        };
        layout.fields.reserve(value.fields.size());
        for (auto const& field : value.fields) {
            layout.fields.push_back(
                {.name = field.name,
                 .logical_type = codegen::resolve_type(field.type, manifest.types).spelling,
                 .bit_width = static_cast<std::uint32_t>(field.bits),
                 .kind = field_kind(field.kind)});
        }
        add_type_representation(result.type_representations,
                                result.diagnostics,
                                qualified_name(module.settings, value.name),
                                layout.storage_type);
        add_type_representation(
            result.type_representations, result.diagnostics, value.name, layout.storage_type);
        if (!result.catalog.add(std::move(layout))) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Duplicate packed layout identity in LispB module '" +
                                              module.settings.name + "'."});
        }
    }
}

void add_soa_module(CatalogLoadResult& result,
                    codegen::SoaModuleSchema const& module,
                    codegen::Manifest const& manifest) {
    if (module.backend != codegen::SoaBackend::standard_library) {
        return;
    }
    for (auto const& schema : module.structs) {
        bool has_nested{};
        for (auto const& member : schema.members) {
            has_nested = has_nested || member.kind == codegen::SoaMemberKind::nested;
        }
        if (has_nested) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Standard-library SoA '" + schema.name +
                     "' uses nested members, which are unsupported in the flat layout planner."});
            continue;
        }

        SoaLayout layout{
            .id = {.kind = SchemaKind::standard_library_soa,
                   .module_name = module.settings.name,
                   .schema_name = schema.name},
            .columns = {},
            .related_storage_name = schema.single_allocation,
        };
        layout.columns.reserve(schema.members.size());
        for (auto const& member : schema.members) {
            auto const resolved{codegen::resolve_type(member.type, manifest.types)};
            layout.columns.push_back(
                {.name = member.name, .logical_type = codegen::native_spelling(resolved.spelling)});
        }
        if (!result.catalog.add(std::move(layout))) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Duplicate SoA layout identity in LispB module '" + module.settings.name + "'."});
        }
    }
}

void add_enum_module(CatalogLoadResult& result,
                     codegen::EnumModuleSchema const& module,
                     codegen::Manifest const& manifest) {
    for (auto const& schema : module.enums) {
        auto const underlying{codegen::native_spelling(
            codegen::resolve_type(schema.underlying_type, manifest.types).spelling)};
        add_type_representation(result.type_representations,
                                result.diagnostics,
                                qualified_name(module.settings, schema.name),
                                underlying);
        add_type_representation(
            result.type_representations, result.diagnostics, schema.name, underlying);
    }
}

void add_vector_module(CatalogLoadResult& result,
                       codegen::VectorModuleSchema const& module,
                       codegen::Manifest const& manifest) {
    if (module.backend != codegen::SoaBackend::standard_library) {
        return;
    }

    auto const type{codegen::native_spelling(
        codegen::resolve_type(module.value_type, manifest.types).spelling)};
    SoaLayout layout{
        .id = {.kind = SchemaKind::standard_library_soa,
               .module_name = module.settings.name,
               .schema_name = module.storage_name},
        .columns = {},
        .related_storage_name = std::nullopt,
    };
    layout.columns.reserve(module.components.size());
    for (auto const& component : module.components) {
        layout.columns.push_back({.name = component, .logical_type = type});
    }
    if (!result.catalog.add(std::move(layout))) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Duplicate vector SoA layout identity in LispB module '" +
                                          module.settings.name + "'."});
    }
}

} // namespace

auto load_lispb_catalog(std::filesystem::path const& project_path, std::string const& target_name)
    -> CatalogLoadResult {
    CatalogLoadResult result;
    try {
        auto const project{lispb::load_project(project_path)};
        auto const target_found{project.targets.find(target_name)};
        if (target_found == project.targets.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "LispB project has no target named '" + target_name + "'."});
            return result;
        }
        auto const* target{std::get_if<lispb::CppSchemaTarget>(&target_found->second)};
        if (target == nullptr) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "LispB target '" + target_name + "' is not a C++ schema target."});
            return result;
        }

        std::vector<std::filesystem::path> sources;
        sources.reserve(target->sources.size());
        for (auto const& source : target->sources) {
            sources.push_back(project.root / source);
        }
        auto const manifest{codegen::load_sources(project.root / target->types, sources)};
        codegen::validate_manifest(manifest);

        for (auto const& module : manifest.modules) {
            std::visit(
                [&](auto const& typed_module) {
                    using Module = std::decay_t<decltype(typed_module)>;
                    if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                        add_packed_module(result, typed_module, manifest);
                    } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                        add_soa_module(result, typed_module, manifest);
                    } else if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                        add_enum_module(result, typed_module, manifest);
                    } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                        add_vector_module(result, typed_module, manifest);
                    }
                },
                module);
        }
        result.loaded = true;
    } catch (std::exception const& error) {
        result.diagnostics.push_back({DiagnosticSeverity::error, error.what()});
    }
    return result;
}

} // namespace ioj::layout
