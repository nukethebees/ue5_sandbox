#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/team.h>
#include <ioj/ui/types.h>

namespace ioj::presentation {
struct EntityOverlayTeamColours {
    EnumArray<Team, ui::Color4f> capital_ship{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    EnumArray<Team, ui::Color4f> fighter{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    EnumArray<Team, ui::Color4f> turret{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
};
}
