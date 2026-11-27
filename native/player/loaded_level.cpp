#include "player/loaded_level.h"

#include <sbx/memory_resource.h>

#include <cassert>
#include <utility>

namespace ioj {
LoadedLevel::LoadedLevel(sim::LevelSimInitData&& data,
                         memory::BumpResource& arena,
                         presentation::LevelPresentationSetup setup)
    : simulation{std::move(data), arena}
    , presentation_setup{std::move(setup)} {}

LevelStorage::LevelStorage()
    : arena_{memory::BumpResource::default_capacity_bytes, *sbx::memory::mimalloc_resource()} {}

void LevelStorage::load(sim::LevelSimInitData&& data, presentation::LevelPresentationSetup setup) {
    assert(!level_);
    try {
        level_.emplace(std::move(data), arena_, std::move(setup));
    } catch (...) {
        arena_.reset();
        throw;
    }
}
void LevelStorage::unload() {
    level_.reset();
    arena_.reset();
}
}
