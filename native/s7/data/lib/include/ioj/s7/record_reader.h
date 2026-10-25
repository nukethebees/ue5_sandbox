#pragma once

#include <ioj/parse_enum.h>
#include <ioj/s7/field_reader.h>

namespace ioj::s7 {
template <typename Field, std::size_t N, typename Visit>
void read_record(DecodeContext context,
                 std::string_view const tag,
                 std::array<std::string_view, N> const& names,
                 Visit visit) {
    auto const& reader{context.reader};
    auto const node{context.node};
    auto const path{context.path};
    auto& errors{context.errors};
    auto const children{reader.list(node, path)};
    if (!children) {
        append_errors(errors, children.error());
        return;
    }
    auto const& ast{reader.ast()};
    if (children->empty() || ast.node((*children)[0]).kind != NodeKind::Keyword ||
        ast.text((*children)[0]) != tag) {
        errors.push_back({AstErrorCode::ExpectedRecord,
                          std::string{path},
                          std::format("Expected a '{}' record", tag)});
        return;
    }
    RecordMask<Field> seen;
    auto const count{children->size()};
    for (std::size_t i{1}; i < count; i += 2) {
        auto const key{(*children)[i]};
        auto const location{std::format("{}[{}]", path, i)};
        if (i + 1 == count) {
            errors.push_back({AstErrorCode::MissingValue, location, "Property has no value"});
        }
        if (ast.node(key).kind != NodeKind::Keyword) {
            errors.push_back(
                {AstErrorCode::ExpectedKeyword, location, "Expected a property keyword"});
            continue;
        }
        auto const name{ast.text(key)};
        auto const field{name.empty() ? std::optional<Field>{}
                                      : try_parse_enum<Field>(name, names)};
        if (!field) {
            errors.push_back({AstErrorCode::UnknownProperty,
                              field_path(path, name),
                              std::format("Unknown property ':{}'", name)});
            continue;
        }
        assert(static_cast<std::size_t>(*field) < static_cast<std::size_t>(Field::Count));
        if (seen.has(*field)) {
            errors.push_back({AstErrorCode::DuplicateProperty,
                              location,
                              std::format("Duplicate property ':{}'", name)});
            continue;
        }
        seen.set(*field);
        if (i + 1 < count) {
            visit(*field, (*children)[i + 1], field_path(path, name));
        }
    }
}

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
        read_record<Field>(
            {context.reader, node, context.path, context.mode, context.errors},
            tag,
            names,
            [&](Field field, NodeIndex value, std::string_view location) {
                seen.set(field);
                visit(field,
                      FieldReader{{context.reader, value, location, context.mode, context.errors}});
            });
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
