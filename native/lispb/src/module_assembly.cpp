#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "record_reader_lowering.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/module_settings_store.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {

auto assemble_module(LoweringContext& context,
                     ModuleSettingsId settings,
                     std::span<DeclarationEmission> emissions) -> std::vector<ModuleId> {
    auto& ir{context.ir};
    NodeListBuilder header{ir};
    NodeListBuilder source{ir};
    header.add(ir.include_dependencies(), 2);
    if (!context.declarations.module_settings_store
             .prelude_lines[context.declarations.module_settings_store.index(settings)]
             .empty()) {
        header.add(
            ir.raw_lines(context.declarations.items(
                context.declarations.module_settings_store
                    .prelude_lines[context.declarations.module_settings_store.index(settings)])),
            2);
    }
    if (context.declarations.module_settings_store
            .source[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        source.add(
            ir.include(ir.store_text(source_include(context, settings)), IncludeStyle::quoted), 2);
        if (std::ranges::any_of(emissions, [](DeclarationEmission const& emission) {
                return emission.source_dependencies;
            })) {
            source.add(ir.include_dependencies(), 2);
        }
    }

    std::vector<ModuleId> additional;
    bool format_generated{};
    NodeListBuilder header_declarations{ir};
    NodeListBuilder header_generated_include{ir};
    NodeListBuilder header_after{ir};
    NodeListBuilder source_definitions{ir};
    NodeListBuilder header_global{ir};
    NodeListBuilder header_tail{ir};
    NodeListBuilder source_global{ir};
    NodeListBuilder source_tail{ir};
    NodeListBuilder reader_declarations{ir};
    NodeListBuilder reader_definitions{ir};
    std::vector<StringId> reader_includes;
    bool has_header{};
    bool has_source{};
    bool has_header_global{};
    bool has_header_after{};
    bool has_header_tail{};
    bool has_source_global{};
    bool has_source_tail{};
    std::optional<std::string> tail_namespace;
    for (auto& emission : emissions) {
        if (!emission.reader_declarations.empty()) {
            reader_declarations.append(std::move(emission.reader_declarations)).new_lines(2);
            reader_definitions.append(std::move(emission.reader_definitions)).new_lines(2);
            for (auto const include : emission.reader_includes) {
                if (std::ranges::find(reader_includes, include) == reader_includes.end()) {
                    reader_includes.push_back(include);
                }
            }
        }
        header.append(std::move(emission.header_prefix));
        header_generated_include.append(std::move(emission.header_generated_include));
        if (!emission.header.empty()) {
            if (has_header) {
                header_declarations.new_lines(2);
            }
            header_declarations.append(std::move(emission.header));
            has_header = true;
        }
        auto append_section = [](NodeListBuilder& builder, Nodes& nodes, bool& started) {
            if (nodes.empty()) {
                return;
            }
            if (started) {
                builder.new_lines(2);
            }
            builder.append(std::move(nodes));
            started = true;
        };
        append_section(header_global, emission.header_global, has_header_global);
        append_section(header_after, emission.header_after, has_header_after);
        append_section(header_tail, emission.header_tail, has_header_tail);
        append_section(source_global, emission.source_global, has_source_global);
        append_section(source_tail, emission.source_tail, has_source_tail);
        if (emission.tail_namespace.has_value()) {
            if (tail_namespace.has_value() && tail_namespace != emission.tail_namespace) {
                throw std::invalid_argument{"Conflicting declaration helper namespaces"};
            }
            tail_namespace = emission.tail_namespace;
        }
        if (!emission.source.empty()) {
            if (has_source) {
                source_definitions.new_lines(2);
            }
            source_definitions.append(std::move(emission.source));
            has_source = true;
        }
        format_generated = format_generated || emission.format_generated;
        additional.insert(additional.end(),
                          emission.additional_modules.begin(),
                          emission.additional_modules.end());
    }
    auto generated_include{header_generated_include.build()};
    if (!generated_include.empty()) {
        header.new_lines(2).append(std::move(generated_include)).new_lines(2);
    }
    auto header_nodes{header_declarations.build()};
    auto after_nodes{header_after.build()};
    auto source_nodes{source_definitions.build()};
    if (context.declarations.module_settings_store
            .namespace_name[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        if (!header_nodes.empty()) {
            header.add(ir.namespace_node(
                *context.declarations.module_settings_store
                     .namespace_name[context.declarations.module_settings_store.index(settings)],
                std::move(header_nodes)));
        }
    } else {
        header.append(std::move(header_nodes));
    }
    header.append(header_global.build());
    if (has_header_global && !after_nodes.empty()) {
        header.new_lines(2);
    }
    if (context.declarations.module_settings_store
            .namespace_name[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        if (!after_nodes.empty()) {
            header.add(ir.namespace_node(
                *context.declarations.module_settings_store
                     .namespace_name[context.declarations.module_settings_store.index(settings)],
                std::move(after_nodes)));
        }
    } else {
        header.append(std::move(after_nodes));
    }
    auto header_tail_nodes{header_tail.build()};
    if (!header_tail_nodes.empty()) {
        if (tail_namespace.has_value() && !tail_namespace->empty()) {
            header.add(
                ir.namespace_node(ir.strings.intern(*tail_namespace), std::move(header_tail_nodes)),
                2);
        } else {
            header.append(std::move(header_tail_nodes));
        }
    }
    auto internal_nodes{source_global.build()};
    if (!internal_nodes.empty()) {
        source.add(ir.namespace_node(ir.strings.intern(""), std::move(internal_nodes)), 2);
    }
    if (context.declarations.module_settings_store
            .namespace_name[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        if (!source_nodes.empty()) {
            source.add(ir.namespace_node(
                *context.declarations.module_settings_store
                     .namespace_name[context.declarations.module_settings_store.index(settings)],
                std::move(source_nodes)));
        }
    } else {
        source.append(std::move(source_nodes));
    }
    auto source_tail_nodes{source_tail.build()};
    if (!source_tail_nodes.empty()) {
        if (tail_namespace.has_value() && !tail_namespace->empty()) {
            source.add(
                ir.namespace_node(ir.strings.intern(*tail_namespace), std::move(source_tail_nodes)),
                2);
        } else {
            source.append(std::move(source_tail_nodes));
        }
    }
    auto const& settings_store{context.declarations.module_settings_store};
    auto const settings_row{settings_store.index(settings)};
    auto const include_order{
        context.declarations.items(settings_store.include_order[settings_row])};
    auto const header_file{ir.files.append(settings_store.header[settings_row], header.build())};
    auto const header_row{ir.files.index(header_file)};
    ir.files.flags[header_row].set_clang_format_off(!format_generated);
    ir.files.flags[header_row].set_format_generated(format_generated);
    ir.files.set_include_order(header_file, include_order);
    CppFileId source_file;
    if (auto const path{settings_store.source[settings_row]}) {
        source_file = ir.files.append(*path, source.build());
        auto const source_row{ir.files.index(source_file)};
        ir.files.flags[source_row].set_pragma_once(false);
        ir.files.flags[source_row].set_clang_format_off(!format_generated);
        ir.files.flags[source_row].set_format_generated(format_generated);
        ir.files.set_include_order(source_file, include_order);
    }
    auto const result{
        ir.modules.append(settings_store.name[settings_row], header_file, source_file)};

    auto reader_nodes{reader_declarations.build()};
    if (!reader_nodes.empty()) {
        NodeListBuilder reader{ir};
        reader.add(
            ir.include(ir.strings.intern(source_include(context, settings)), IncludeStyle::quoted));
        reader.add(ir.include(ir.strings.intern("ioj/s7/reader.h"), IncludeStyle::system));
        for (auto const include : reader_includes) {
            reader.add(ir.include(include, IncludeStyle::quoted));
        }
        reader.new_lines(2);
        auto definitions{reader_definitions.build()};
        reader_nodes.insert(reader_nodes.end(), definitions.begin(), definitions.end());
        reader.add(ir.namespace_node(ir.strings.intern("ioj::s7"), reader_nodes));
        auto const reader_path{
            reader_header_path(context.text(settings_store.header[settings_row]))};
        auto const file{ir.files.append(ir.strings.intern(reader_path), reader.build())};
        ir.files.flags[ir.files.index(file)].set_format_generated(true);
        additional.push_back(ir.modules.append(
            ir.strings.intern(context.text(settings_store.name[settings_row]) + "Reader"),
            file,
            {}));
    }

    std::vector<ModuleId> modules;
    modules.reserve(additional.size() + 1);
    modules.push_back(result);
    modules.insert(modules.end(), additional.begin(), additional.end());
    return modules;
}

} // namespace lispb::codegen::detail
