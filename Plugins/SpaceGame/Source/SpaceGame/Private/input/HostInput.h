#pragma once

#include <ioj/input/presentation_events.h>

namespace ioj::host {
void present_input(MenuKey const& key);
void present_input(MenuText const& text);
void present_input(MenuPointer const& pointer);
}
