#pragma once

namespace mesh_gen {

struct ValidParameters {
    [[nodiscard]] constexpr auto operator()(auto const& input) const -> bool {
        return input.error().empty();
    }
};

}
