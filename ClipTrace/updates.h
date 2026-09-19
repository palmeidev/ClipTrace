#pragma once

#include <windows.h>
#include <cstdint>
#include <string>

namespace ClipTraceUpdates {
constexpr UINT AvailableMessage = WM_APP + 4;
constexpr UINT ErrorMessage = WM_APP + 5;
constexpr UINT ReadyMessage = WM_APP + 6;

struct Release {
    std::wstring tag;
    std::wstring url;
    std::wstring digest;
    uint64_t size = 0;
};

void CheckAsync(HWND window, bool force = false);
bool InstallAsync(HWND window, Release release);
bool RunHelperIfRequested(int& exitCode);
}
