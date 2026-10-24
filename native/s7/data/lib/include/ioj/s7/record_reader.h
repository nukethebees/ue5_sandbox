#pragma once

#include <ioj/parse_enum.h>
#include <ioj/s7/field_reader.h>

namespace ioj::s7 {
template <typename Field, bool Patch = false, std::size_t N, typename Visit>
void decode_record(DecodeContext context,
                   std::string_view tag,
                   std::array<std::string_view, N> const& names,
                   Visit visit,
                   RecordMask<Field> const& required = {}) {
    static_assert(N == static_cast<std::size_t>(Field::Count));
    if constexpr (Patch) {
        context.mode = ReadMode::Override;
    }
    auto const seen{read_record<Field>(
        context.reader,
        context.node,
        tag,
        context.path,
        [&](std::string_view name) { return try_parse_enum<Field>(name, names); },
        [&](Field field, NodeIndex value, std::string_view location) {
            visit(field,
                  FieldReader<Patch>{
                      {context.reader, value, location, context.mode, context.errors}});
        })};

    if (!seen) {
        append_errors(context.errors, seen.error());
        return;
    }
    if (context.mode == ReadMode::Create) {
        require_fields<Field>(*seen, required, names, context.path, context.errors);
    }
}

template <typename T, typename Patch = void>
struct RecordReader {
    using value_type = T;

    static auto read(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<T> {
        if constexpr (std::same_as<T, Patch>) {
            return read_override(reader, node, path, T{});
        } else {
            AstDiagnostics errors;
            DecodeContext const context{reader, node, path, ReadMode::Create, errors};
            if constexpr (std::default_initializable<T>) {
                T value{};
                TypeReader<T>::decode(context, value);
                if (!errors.empty()) {
                    return std::unexpected{std::move(errors)};
                }
                return value;
            } else {
                return TypeReader<T>::construct(context);
            }
        }
    }
};
}
