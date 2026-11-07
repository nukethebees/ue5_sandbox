#pragma once

#include <ioj/constrained_value.h>

#include <mesh_gen/valid_parameters.h>

#include <optional>
#include <source_location>

namespace mesh_gen {

inline constexpr auto parameters_policy{ioj::ConstrainedValuePolicy{
    ValidParameters{}, "Mesh parameters violate their shape constraints"}};

// Keep the policy value out of native/Unreal function signatures because their mangling differs.
template <typename Input>
class CheckedParameters : public ioj::ConstrainedValue<Input, parameters_policy> {
    using Base = ioj::ConstrainedValue<Input, parameters_policy>;

  public:
    using Base::Base;

    constexpr CheckedParameters() = default;

    [[nodiscard]] static auto
        make(Input const& input,
             std::source_location const location = std::source_location::current())
            -> CheckedParameters {
        return CheckedParameters{Base::make(input, location)};
    }

    [[nodiscard]] static constexpr auto try_make(Input const& input)
        -> std::optional<CheckedParameters> {
        auto const checked{Base::try_make(input)};
        if (!checked) {
            return std::nullopt;
        }
        return CheckedParameters{*checked};
    }

  private:
    explicit constexpr CheckedParameters(Base const& checked)
        : Base{checked} {}
};

}
