#pragma once

#include <SpaceGame/settings/ControlBindingAddress.h>

#include <InputCoreTypes.h>

namespace ioj {
struct FControlChordBindingView {
    FControlBindingAddress address;
    FKey current_key;
    FKey default_key;
};
}
