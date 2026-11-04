#pragma once

#include <ioj/team_array.h>
#include <ioj/ui/types.h>

namespace ioj::presentation {
struct EntityOverlayTeamColours {
    TeamArray<ui::Color4f> capital_ship{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    TeamArray<ui::Color4f> fighter{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    TeamArray<ui::Color4f> turret{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
};
}
