#include <ioj/layout/lispb_adapter.hpp>

#include <codegen/schema.h>
#include <codegen/source_loader.h>
#include <codegen/validation.h>
#include <lispb/project.h>

#include <exception>
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

void add_packed_module(SchemaCatalog& catalog,
                       std::vector<Diagnostic>& diagnostics,
                       codegen::PackedValueModuleSchema const& module,
                       codegen::Manifest const& manifest) {
    for (auto const& value : module.values) {
        PackedLayout layout{
            .id = {.kind = SchemaKind::packed_value,
                   .module_name = module.settings.name,
                   .schema_name = value.name},
            .storage_type =
                codegen::native_spelling(codegen::resolve_type(value.storage_type, manifest.types).spelling),
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
        if (!catalog.add(std::move(layout))) {
            diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Duplicate packed layout identity in LispB module '" + module.settings.name + "'."});
        }
    }
}

void add_soa_module(SchemaCatalog& catalog,
                    std::vector<Diagnostic>& diagnostics,
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
            diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Standard-library SoA '" + schema.name +
                     "' uses nested members, which are unsupported in layout planner V1."});
            continue;
        }

        SoaLayout layout{
            .id = {.kind = SchemaKind::standard_library_soa,
                   .module_name = module.settings.name,
                   .schema_name = schema.name},
            .columns = {},
        };
        layout.columns.reserve(schema.members.size());
        for (auto const& member : schema.members) {
            auto const resolved{codegen::resolve_type(member.type, manifest.types)};
            layout.columns.push_back(
                {.name = member.name,
                 .logical_type = codegen::native_spelling(resolved.spelling)});
        }
        if (!catalog.add(std::move(layout))) {
            diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Duplicate SoA layout identity in LispB module '" + module.settings.name + "'."});
        }
    }
}

} // namespace

auto load_lispb_catalog(std::filesystem::path const& project_path,
                        std::string const& target_name) -> CatalogLoadResult {
    CatalogLoadResult result;
    try {
        auto const project{lispb::load_project(project_path)};
        auto const target_found{project.targets.find(target_name)};
        if (target_found == project.targets.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "LispB project has no target named '" + target_name + "'."});
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
                        add_packed_module(result.catalog, result.diagnostics, typed_module, manifest);
                    } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                        add_soa_module(result.catalog, result.diagnostics, typed_module, manifest);
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
