#pragma once

#include "Containers/Array.h"
#include "UObject/NameTypes.h"

class AActor;
class USceneComponent;

namespace ml {
template <typename T>
void destroy_components_array(TArray<T>& components) {
    for (auto& component : components) {
        if (!IsValid(component)) {
            continue;
        }

        component->Modify();
        component->DestroyComponent();
    }

    components.Reset();
};
template <typename T>
void destroy_components_at_array_end(TArray<T>& components, int32 const count) {
    if (count < 1) {
        return;
    }

    auto const n_components_at_start{components.Num()};
    if (n_components_at_start <= count) {
        destroy_components_array(components);
        return;
    }

    auto const first_index{n_components_at_start - count};

    for (int32 i{first_index}; i < n_components_at_start; ++i) {
        auto& component{components[i]};

        if (!IsValid(component)) {
            continue;
        }

        component->Modify();
        component->DestroyComponent();
    }

    components.RemoveAt(first_index, count, EAllowShrinking::No);
};

template <typename... Components>
auto register_components(Components*... components) -> void {
    ((components ? components->RegisterComponent() : void()), ...);
}

template <typename TComponent>
auto create_attached_instance_component(AActor& owner, FName const name, USceneComponent& parent)
    -> TComponent* {
    auto* const component{NewObject<TComponent>(&owner, name)};

    component->SetupAttachment(&parent);
    component->RegisterComponent();
    owner.AddInstanceComponent(component);

    return component;
}
}
