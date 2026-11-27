#pragma once

#include <ioj/input/logical/flight_bindings.h>
#include <ioj/input/logical/flight_controls.h>
#include <ioj/s7/reader.h>

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::s7 {
template <>
struct TypeReader<FlightBindings> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<FlightBindings> {
        auto entries{reader.list(node, path)};
        if (!entries) {
            return std::unexpected{std::move(entries.error())};
        }

        auto bindings{default_flight_bindings()};
        for (auto entry : *entries) {
            auto pair{s7::read<std::array<std::string, 2>>(reader, entry, path)};
            if (!pair) {
                return std::unexpected{std::move(pair.error())};
            }

            auto const mapping{try_parse_serialized_flight_mapping((*pair)[0])};
            auto const key{try_parse_serialized_physical_key((*pair)[1])};
            if (!mapping || !key ||
                (*key != PhysicalKey::Unbound && !accepts_binding(*mapping, *key))) {
                return read_error(
                    AstErrorCode::InvalidType,
                    path,
                    std::format("Invalid binding '{}' for '{}'", (*pair)[1], (*pair)[0]));
            }
            bindings[*mapping] = *key;
        }
        return bindings;
    }
};
}
