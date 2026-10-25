#pragma once

#include <ioj/parse_enum.h>
#include <ioj/s7/field_reader.h>

namespace ioj::s7 {
template <typename Field, std::size_t N, typename Visit>
void decode_record(DecodeContext context,
                   std::string_view tag,
                   std::array<std::string_view, N> const& names,
                   Visit visit,
                   RecordMask<Field> const& required = {}) {
    static_assert(N == static_cast<std::size_t>(Field::Count));
    RecordMask<Field> seen;
    auto const& ast{context.reader.ast()};
    auto const decode{[&](auto const& self, NodeIndex node) -> void {
        if (ast.node(node).kind == NodeKind::List) {
            auto const children{ast.children(node)};
            if (!children.empty() && ast.node(children.front()).kind == NodeKind::Keyword &&
                ast.text(children.front()) == "settings-layers") {
                for (auto const layer : children.subspan(1)) {
                    self(self, layer);
                }
                return;
            }
        }
        auto const fields{read_record<Field>(
            context.reader,
            node,
            tag,
            context.path,
            [&](std::string_view name) { return try_parse_enum<Field>(name, names); },
            [&](Field field, NodeIndex value, std::string_view location) {
                seen.set(field);
                visit(field,
                      FieldReader{{context.reader, value, location, context.mode, context.errors}});
            })};
        if (!fields) {
            append_errors(context.errors, fields.error());
        }
    }};
    decode(decode, context.node);

    if (context.mode == ReadMode::Create) {
        require_fields<Field>(seen, required, names, context.path, context.errors);
    }
}

template <typename T>
struct RecordReader {
    using value_type = T;

    static auto read(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<T> {
        AstDiagnostics errors;
        DecodeContext const context{reader, node, path, ReadMode::Create, errors};
        if constexpr (std::default_initializable<T>) {
            T value{};
            decode_field(context, value);
            if (!errors.empty()) {
                return std::unexpected{std::move(errors)};
            }
            return value;
        } else {
            return TypeReader<T>::construct(context);
        }
    }
};
}
