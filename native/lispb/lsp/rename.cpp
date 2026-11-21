#include "analysis.h"

#include <kernel_codegen/compiler.h>

#include <algorithm>

namespace lispb::lsp {

auto Analysis::rename(Document const& doc,
                      sexpr::SourceSpan declaration,
                      sexpr::FormId use,
                      Method method,
                      Json const& params) const -> std::expected<Json, std::string> {
    auto const& storage{compiler_.storage()};
    auto const& syntax{storage.syntax};
    bool const prepare{method == Method::prepare_rename};
    auto const name{prepare ? std::string_view{} : params.at("newName").get<std::string_view>()};
    auto const old{syntax.text(use)};
    // A failed frontend can leave only the references observed before the error.
    for (auto const& issue : diagnostics_) {
        if (storage.diagnostics.severity(issue.id) == DiagnosticSeverity::error) {
            if (prepare) {
                return nullptr;
            }
            return std::unexpected{
                "Resolve the target's compiler errors before renaming a symbol."};
        }
    }

    bool supported{};
    auto const& material{compiler_.material()};
    for (auto param : material.parameters.handles()) {
        auto const i{param.value};
        if (material.text(material.parameters.name[i]) != old ||
            material.parameters.span[i].path != storage.sources.path_id(doc.file)) {
            continue;
        }
        auto const symbol{std::ranges::find_if(doc.symbols, [&](Symbol const& candidate) {
            return syntax.token_span(candidate.name).offset == declaration.offset &&
                   try_parse_serialized_form_head(syntax.head(candidate.form)) ==
                       FormHead::parameter;
        })};
        if (symbol == doc.symbols.end()) {
            continue;
        }
        supported = true;
        if (!prepare && name != old) {
            if (!material_synth::valid_identifier(name)) {
                return std::unexpected{"A material parameter name must be a valid identifier."};
            }
            auto matches = [&](StringId id) { return material.text(id) == name; };
            if (std::ranges::any_of(material.parameters.name, matches) ||
                std::ranges::any_of(material.bindings.name, matches)) {
                return std::unexpected{
                    "A parameter or binding already uses that name in this material."};
            }
        }
        break;
    }
    auto const& kernel{compiler_.kernel()};
    auto const count{kernel.modules.size()};
    for (std::uint32_t i{}; i < count && !supported; ++i) {
        for (auto op : kernel.modules.operations(kernel_codegen::detail::ModuleId{i})) {
            auto const operands{kernel.operations.operands(op)};
            if (!std::ranges::any_of(operands, [&](auto id) {
                    auto const span{kernel.operands.span(id)};
                    return span.file == declaration.file && span.offset == declaration.offset;
                })) {
                continue;
            }
            supported = true;
            if (!prepare && name != old) {
                if (!kernel_codegen::detail::is_identifier(name)) {
                    return std::unexpected{"A kernel operand name must be a valid C++ identifier."};
                }
                if (storage.strings.view(kernel.operations.output(op)) == name ||
                    std::ranges::any_of(operands, [&](auto id) {
                        return storage.strings.view(kernel.operands.name(id)) == name;
                    })) {
                    return std::unexpected{
                        "An operand or output already uses that name in this operation."};
                }
            }
            break;
        }
    }
    if (!supported) {
        return nullptr;
    }
    if (prepare) {
        return Json{{"range", range(storage.sources, syntax.token_span(use))},
                    {"placeholder", old}};
    }
    Json edits = Json::array();
    for (auto span : reference_spans(declaration, true)) {
        edits.push_back({{"range", range(storage.sources, span)}, {"newText", name}});
    }
    return Json{{"changes", {{file_uri(doc.path), std::move(edits)}}}};
}
}
