#pragma once

namespace lispb::codegen::detail {

template <typename Store, typename Id, typename... Columns>
auto clone_schema_row(Store& store, Id id, Columns&... columns) -> Id {
    auto const source{store.index(id)};
    auto const result{store.append()};
    auto const destination{store.index(result)};
    ((columns[destination] = columns[source]), ...);
    return result;
}

template <typename... Columns>
void clear_schema_columns(Columns&... columns) {
    (columns.clear(), ...);
}

}
