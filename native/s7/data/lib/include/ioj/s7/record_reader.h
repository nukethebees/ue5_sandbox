#pragma once

#include <ioj/s7/reader.h>

namespace ioj::s7 {
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
