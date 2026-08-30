#include "SpaceGame/support/mesh.h"

#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/PrimitiveComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>

namespace ml {
auto get_static_mesh(UPrimitiveComponent const* const component) -> UStaticMesh const* {
    if (component == nullptr) {
        return nullptr;
    }

    if (!IsValid(component)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot get static mesh from an invalid component"));
        return nullptr;
    }

    auto const* const static_mesh_component{Cast<UStaticMeshComponent>(component)};
    if (!IsValid(static_mesh_component)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot get static mesh from component %s: component is not a static mesh "
                    "component"),
               *component->GetName());
        return nullptr;
    }

    return static_mesh_component->GetStaticMesh();
}

auto get_mesh_sphere_bounds(UStaticMesh const& mesh) -> float {
    return static_cast<float>(mesh.GetBounds().SphereRadius);
}
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float {
    auto const static_mesh{mesh.GetStaticMesh()};
    check(static_mesh);
    return get_mesh_sphere_bounds(*static_mesh);
}
}
