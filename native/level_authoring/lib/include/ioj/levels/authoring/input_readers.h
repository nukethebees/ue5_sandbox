#pragma once

#include <ioj/levels/authoring/expr_input.h>
#include <ioj/levels/authoring/identifier_reader.h>
#include <ioj/settings/settings.h>

namespace ioj::s7 {
template <>
struct TypeReader<levels::ObjectiveName> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::ObjectiveName> {
        return s7::read<levels::EntityId>(reader, node, path).transform([](auto id) {
            return levels::ObjectiveName{std::move(id.value)};
        });
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
struct TypeReader<levels::authoring::ExprInput> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::authoring::ExprInput>;
};

template <>
struct TypeReader<levels::authoring::NamedExprInput> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::authoring::NamedExprInput>;
};

template <>
struct TypeReader<levels::ObjectiveDependency> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::ObjectiveDependency>;
};

template <>
struct TypeReader<levels::authoring::UnlockInput> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<levels::authoring::UnlockInput>;
};

template <>
struct TypeReader<settings::Settings> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<settings::Settings>;
};
}
