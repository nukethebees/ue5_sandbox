#pragma once

namespace ml::single_allocation_experiment {

template <typename View>
auto array_columns(View view) {
    if constexpr (requires { view.columns(); }) {
        return view.columns();
    } else {
        return view;
    }
}

// Shared traversal for test assertions and allocation diagnostics, not owner mutations.
template <typename View, typename Func>
void each_leaf(View&& view, Func const& func) {
    if constexpr (requires { view.GetData(); }) {
        func(view);
    } else {
        view.apply_arrays([&](auto&&... columns) { (each_leaf(columns, func), ...); });
    }
}

}
