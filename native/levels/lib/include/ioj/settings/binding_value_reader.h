#pragma once

#include <ioj/input/key_code.h>
#include <ioj/s7/reader.h>

namespace ioj::s7 {
template <>
struct TypeReader<FlightMapping> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<FlightMapping> {
        auto name{s7::read<std::string>(reader, node, path)};
        if (!name) {
            return std::unexpected{std::move(name.error())};
        }
        if (auto mapping{input::parse_mapping(*name)}) {
            return *mapping;
        }
        return read_error(
            AstErrorCode::UnknownSymbol, path, std::format("Unknown control mapping '{}'", *name));
    }
};
template <>
struct TypeReader<input::KeyCode> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<input::KeyCode> {
        auto name{s7::read<std::string>(reader, node, path)};
        if (!name) {
            return std::unexpected{std::move(name.error())};
        }
        return input::parse_key(*name);
    }
};
}
