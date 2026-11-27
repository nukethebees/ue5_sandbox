#pragma once

#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/level_init.h>

#include <string>

namespace ioj::levels {
class LevelDefinition;
struct LaunchOptions;
}
namespace ioj::sim {
struct LevelSim;
}
namespace ioj::bridge {
auto make_level_presentation_setup(levels::LevelDefinition const& definition,
                                   std::string source_path,
                                   levels::LaunchOptions const& options)
    -> presentation::LevelPresentationSetup;
auto level_init(presentation::LevelPresentationSetup const& setup, sim::LevelSim const& level)
    -> presentation::LevelInit;
auto level_frame(sim::LevelSim const& level) -> presentation::LevelFrame;
}
