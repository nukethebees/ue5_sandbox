#pragma once

#include <ioj/levels/authoring/identifier_reader.h>
#include <ioj/levels/logic_definition.h>
#include <ioj/s7/raw_node.h>
#include <ioj/settings/settings.h>
#include <ioj/settings/settings_values_reader.h>

namespace ioj::s7 {
template <>
struct TypeReader<RawNode> {
    static auto read(Reader const&, NodeIndex node, std::string_view) -> ReadResult<RawNode> {
        return RawNode{node};
    }
};

// Capture per-entry failures so catalogs retain successfully decoded siblings.
template <typename T>
struct TypeReader<ReadResult<T>> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<ReadResult<T>> {
        return ReadResult<ReadResult<T>>{std::in_place, s7::read<T>(reader, node, path)};
    }
};

template <>
struct TypeReader<levels::LogicDefinition> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::LogicDefinition>;
};

}
