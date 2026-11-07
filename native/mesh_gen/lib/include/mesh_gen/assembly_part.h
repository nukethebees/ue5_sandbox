#pragma once

#include <mesh_gen/generation_request.h>
#include <mesh_gen/transform.h>

#include <span>
#include <string>

namespace mesh_gen {

struct AssemblyPart {
    GenerationRequest mesh;
    Transform transform;
    bool visible{true};
    bool locked{};
};

void append_transformed_mesh(MeshData& destination,
                             MeshData const& source,
                             Transform const& transform);
[[nodiscard]] auto generate_assembly(std::span<AssemblyPart const> parts) -> MeshData;
[[nodiscard]] auto describe_assembly(std::span<AssemblyPart const> parts) -> std::string;

}
