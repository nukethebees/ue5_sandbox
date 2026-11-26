#pragma once

#include "player/windows/system.h"

// Win64 imports from UELibraryAPI.h, whose public header requires UBT platform headers.
extern "C" {
    __declspec(dllimport) int
        UELibrary_InitW(HINSTANCE instance, HWND window, wchar_t const* arguments);
    __declspec(dllimport) int UELibrary_Tick();
    __declspec(dllimport) LRESULT UELibrary_WndProc(HWND window,
                                                    UINT message,
                                                    WPARAM wp,
                                                    LPARAM lp);
    __declspec(dllimport) int UELibrary_Shutdown();
}
