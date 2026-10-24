#pragma once

#include <ioj/frequency.h>
#include <ioj/s7/reader.h>

namespace ioj::s7 {
template <>
struct TypeReader<Frequency> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<Frequency> {
        auto value{s7::read<double>(reader, node, path)};
        if (!value) {
            return std::unexpected{std::move(value.error())};
        }
        if (auto frequency{Frequency::try_make(*value)}) {
            return *frequency;
        }
        return read_error(
            AstErrorCode::InvalidNumber, path, "Frequency must be finite and at least 1e-300 Hz");
    }
};
}
