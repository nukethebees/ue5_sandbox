#pragma once

#include <ioj/parse_enum.h>
#include <ioj/s7/field_reader.h>

namespace ioj::s7 {
template <typename Field, std::size_t N, typename Visit>
void decode_record(Reader const& reader,
                   NodeIndex node,
                   std::string_view tag,
                   std::string_view path,
                   std::array<std::string_view, N> const& names,
                   ReadMode mode,
                   AstDiagnostics& errors,
                   Visit visit,
                   RecordMask<Field> const& required = {}) {
    static_assert(N == static_cast<std::size_t>(Field::Count));
    auto const seen{read_record<Field>(
        reader,
        node,
        tag,
        path,
        [&](std::string_view name) { return try_parse_enum<Field>(name, names); },
        [&](Field field, NodeIndex value, std::string_view location) {
            visit(field, FieldReader{reader, value, location, errors, mode});
        })};

    if (!seen) {
        append_errors(errors, seen.error());
        return;
    }
    if (mode == ReadMode::Create) {
        require_fields<Field>(*seen, required, names, path, errors);
    }
}

template <typename T, ReadMode Mode = ReadMode::Create>
struct RecordReader {
    using value_type = T;

    static auto read(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<T> {
        if constexpr (Mode == ReadMode::Override) {
            return read_override(reader, node, path, T{});
        } else if constexpr (std::default_initializable<T>) {
            T value{};
            AstDiagnostics errors;
            TypeReader<T>::decode(reader, node, path, value, Mode, errors);
            if (!errors.empty()) {
                return std::unexpected{std::move(errors)};
            }
            return value;
        } else {
            return TypeReader<T>::construct(reader, node, path);
        }
    }
};
}
