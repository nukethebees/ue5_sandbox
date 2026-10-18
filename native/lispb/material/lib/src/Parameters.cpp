#include <material_gen/Parameters.h>

namespace material_synth {
auto Parameters::append(lispb::StringId name,
                        ValueType type,
                        lispb::StringId texture_path,
                        TextureSamplerType texture_sampler_type,
                        NodeHandle node,
                        SourceSpan span,
                        std::array<double, 4> const& values) -> ParameterId {
    auto const id{ParameterId{static_cast<std::uint32_t>(size())}};
    this->name.push_back(name);
    this->type.push_back(type);
    this->texture_path.push_back(texture_path);
    this->texture_sampler_type.push_back(texture_sampler_type);
    this->node.push_back(node);
    this->span.push_back(span);
    for (std::size_t component{}; component < 4; ++component) {
        default_values[component].push_back(values[component]);
    }
    return id;
}
}
