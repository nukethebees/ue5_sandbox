#include "lowering.h"
#include "lowering_utils.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace codegen::detail {

auto assemble_module(ModuleSettings const& settings, std::span<DeclarationEmission> emissions)
    -> std::vector<Module> {
    NodeListBuilder header;
    NodeListBuilder source;
    header.add(IncludeDependencies{}, 2);
    if (!settings.prelude_lines.empty()) {
        header.add(raw(join_lines(settings.prelude_lines)), 2);
    }
    if (settings.source.has_value()) {
        source.add(Include{source_include(settings), false}, 2);
        if (std::ranges::any_of(emissions, [](DeclarationEmission const& emission) {
                return emission.source_dependencies;
            })) {
            source.add(IncludeDependencies{}, 2);
        }
    }

    std::vector<Module> additional;
    bool format_generated{};
    NodeListBuilder header_declarations;
    NodeListBuilder header_generated_include;
    NodeListBuilder header_after;
    NodeListBuilder source_definitions;
    NodeListBuilder header_global;
    NodeListBuilder header_tail;
    NodeListBuilder source_global;
    NodeListBuilder source_tail;
    bool has_header{};
    bool has_source{};
    bool has_header_global{};
    bool has_header_after{};
    bool has_header_tail{};
    bool has_source_global{};
    bool has_source_tail{};
    std::optional<std::string> tail_namespace;
    for (auto& emission : emissions) {
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
        for (auto& module : emission.additional_modules) {
            additional.push_back(std::move(module));
        }
    }
    auto generated_include{header_generated_include.build()};
    if (!generated_include.empty()) {
        header.new_lines(2).append(std::move(generated_include)).new_lines(2);
    }
    auto header_nodes{header_declarations.build()};
    auto after_nodes{header_after.build()};
    auto source_nodes{source_definitions.build()};
    if (settings.namespace_name.has_value()) {
        if (!header_nodes.empty()) {
            header.add(Namespace{*settings.namespace_name, std::move(header_nodes)});
        }
    } else {
        header.append(std::move(header_nodes));
    }
    header.append(header_global.build());
    if (has_header_global && !after_nodes.empty()) {
        header.new_lines(2);
    }
    if (settings.namespace_name.has_value()) {
        if (!after_nodes.empty()) {
            header.add(Namespace{*settings.namespace_name, std::move(after_nodes)});
        }
    } else {
        header.append(std::move(after_nodes));
    }
    auto header_tail_nodes{header_tail.build()};
    if (!header_tail_nodes.empty()) {
        if (tail_namespace.has_value() && !tail_namespace->empty()) {
            header.add(Namespace{*tail_namespace, std::move(header_tail_nodes)}, 2);
        } else {
            header.append(std::move(header_tail_nodes));
        }
    }
    auto internal_nodes{source_global.build()};
    if (!internal_nodes.empty()) {
        source.add(Namespace{"", std::move(internal_nodes)}, 2);
    }
    if (settings.namespace_name.has_value()) {
        if (!source_nodes.empty()) {
            source.add(Namespace{*settings.namespace_name, std::move(source_nodes)});
        }
    } else {
        source.append(std::move(source_nodes));
    }
    auto source_tail_nodes{source_tail.build()};
    if (!source_tail_nodes.empty()) {
        if (tail_namespace.has_value() && !tail_namespace->empty()) {
            source.add(Namespace{*tail_namespace, std::move(source_tail_nodes)}, 2);
        } else {
            source.append(std::move(source_tail_nodes));
        }
    }
    Module result{
        .name = settings.name,
        .header = CppFile{.path = settings.header,
                          .nodes = header.build(),
                          .clang_format_off = !format_generated,
                          .include_order = settings.include_order,
                          .format_generated = format_generated},
    };
    if (settings.source.has_value()) {
        result.source = CppFile{.path = *settings.source,
                                .nodes = source.build(),
                                .pragma_once = false,
                                .clang_format_off = !format_generated,
                                .include_order = settings.include_order,
                                .format_generated = format_generated};
    }

    std::vector<Module> modules;
    modules.reserve(additional.size() + 1);
    modules.push_back(std::move(result));
    for (auto& module : additional) {
        modules.push_back(std::move(module));
    }
    return modules;
}

} // namespace codegen::detail
