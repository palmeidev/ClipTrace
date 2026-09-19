#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <propkey.h>
#include <propvarutil.h>
#include <uiautomation.h>
#include "icons.h"
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <cstdint>
#include "resource.h"
#include "updates.h"
namespace Gdiplus {
    using std::min;
    using std::max;
}
#include <gdiplus.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "windowsapp.lib")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

namespace {
constexpr int kTitleBarHeight = 40;
constexpr int kCloseWidth = 46;
constexpr COLORREF kWindowBackground = RGB(28, 38, 58);   // #1C263A
constexpr COLORREF kCardBackground = RGB(35, 47, 72);
constexpr COLORREF kCardSelected = RGB(39, 53, 80);
constexpr COLORREF kButtonBackground = RGB(49, 62, 91);
constexpr COLORREF kText = RGB(245, 247, 252);
constexpr COLORREF kMuted = RGB(190, 199, 219);
constexpr COLORREF kAccent = RGB(99, 190, 255);
constexpr COLORREF kSelectedBorder = RGB(224, 231, 245);

HFONT gTitleFont, gHeaderFont, gBodyFont, gSmallFont, gIconFont, gLargeIconFont, gCodeFont;
RECT gCloseRect{};
RECT gClearRect{};
RECT gPauseRect{};
RECT gAboutRect{};
RECT gSettingsRect{};
RECT gSearchRect{};
RECT gClearSearchRect{};
bool gCloseHovered = false;
bool gSettingsHovered = false;
bool gAboutHovered = false;
bool gPauseHovered = false;
bool gSearchHovered = false;
bool gSearchFocused = false;
bool gMonitoringPaused = false;
float gSettingsHoverProgress = 0.0f;
float gSettingsPressProgress = 0.0f;
float gAboutHoverProgress = 0.0f;
float gAboutPressProgress = 0.0f;
float gPauseHoverProgress = 0.0f;
float gPausePressProgress = 0.0f;
float gSearchHoverProgress = 0.0f;
std::wstring gSearchQuery = L"";
int gScrollOffset = 0;

constexpr UINT kGlobalHotkeyId = 1001;
constexpr UINT WM_OPEN_CLIPTRACE = WM_APP + 3;
constexpr UINT WM_TRAYICON = WM_APP + 2;
constexpr UINT IDM_TRAY_OPEN = 2001;
constexpr UINT IDM_TRAY_PAUSE = 2002;
constexpr UINT IDM_TRAY_CLEAR = 2003;
constexpr UINT IDM_TRAY_SETTINGS = 2004;
constexpr UINT IDM_TRAY_EXIT = 2005;
constexpr UINT IDM_TRAY_ABOUT = 2006;
constexpr UINT IDM_TRAY_UPDATE = 2007;
constexpr UINT WM_INSTALL_UPDATE = WM_APP + 7;

Gdiplus::Bitmap* gLogoBitmap = nullptr;
Gdiplus::Bitmap* gDeveloperBitmap = nullptr;

enum class HistoryRetention {
    Never = 0,
    TwoHours = 1,
    OneDay = 2,
    OneWeek = 3,
    OneMonth = 4
};
HistoryRetention gRetentionSetting = HistoryRetention::Never;
bool gStartWithWindows = true;
bool gReplaceWinV = false;
HHOOK gWinVHook = nullptr;
HWND gMainWindow = nullptr;
bool gWinKeyDown = false;
bool gWinVDown = false;
bool gCtrlDown = false;
bool gAltDown = false;
std::unique_ptr<ClipTraceUpdates::Release> gAvailableUpdate;
winrt::Windows::UI::Notifications::ToastNotification gUpdateToast{nullptr};
bool gUpdateBalloonActive = false;

struct ClipItem {
    std::wstring preview;
    std::wstring rawContent;
    std::wstring source;
    std::wstring pageTitle;
    std::wstring pageUrl;
    SYSTEMTIME copiedAt{};
    enum class Type { Text, Link, Image, File, Color, Other } type = Type::Other;
    using BitmapObject = std::remove_pointer_t<HBITMAP>;
    std::shared_ptr<BitmapObject> image;
    bool pinned = false;
    std::wstring imageFile;
};
std::vector<ClipItem> gHistory;
std::vector<RECT> gCardRects;
int gSelectedIndex = 0;
NOTIFYICONDATAW gTrayIcon{};
HICON gLargeAppIcon = nullptr;

struct DetailDialogState {
    ClipItem item;
    HWND owner = nullptr;
    RECT closeRect{};
    RECT urlRect{};
    RECT contentRect{};
    bool urlHovered = false;
    bool closeHovered = false;
    bool contentHovered = false;

    float closeHoverProgress = 0.0f;
    float contentHoverProgress = 0.0f;
    float urlHoverProgress = 0.0f;

    int targetX = 0;
    int targetY = 0;
    float openProgress = 0.0f;
    float closeProgress = 0.0f;
    bool isClosing = false;
};
constexpr UINT_PTR kDetailAnimTimer = 101;
constexpr UINT kDetailAnimInterval = 14;

struct SettingsDialogState {
    HWND owner = nullptr;
    RECT closeRect{};
    RECT clearHistoryRect{};
    RECT retentionTabs[5]{};
    RECT autoStartRowRect{};
    RECT autoStartSwitchRect{};
    RECT winVRowRect{};
    RECT winVSwitchRect{};

    bool closeHovered = false;
    bool clearHovered = false;
    int tabHovered = -1;
    bool switchHovered = false;
    bool winVHovered = false;

    float closeHoverProgress = 0.0f;
    float clearHoverProgress = 0.0f;
    float tabHoverProgress[5]{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float switchToggleProgress = 0.0f;
    float switchHoverProgress = 0.0f;
    float winVToggleProgress = 0.0f;
    float winVHoverProgress = 0.0f;

    int targetX = 0;
    int targetY = 0;
    float openProgress = 0.0f;
    float closeProgress = 0.0f;
    bool isClosing = false;
};
constexpr UINT_PTR kSettingsAnimTimer = 102;
constexpr UINT kSettingsAnimInterval = 14;

std::wstring GetAppDataDirectory();
bool SaveBitmapToFile(HBITMAP hBitmap, const std::wstring& filePath);
bool SaveBitmapToTempFile(HBITMAP hBitmap, std::wstring& outPath);
void SaveHistoryToDisk();
void LoadHistoryFromDisk();
void SaveSettings();
void LoadSettings();
bool IsAutoStartEnabled();
void SetAutoStartEnabled(bool enable);
bool SetWinVReplacement(bool enable);
bool ApplyAutoCleanup();
void ClearAllHistory();
void ShowSettings(HWND hwnd);
void ShowAbout(HWND hwnd);
void ShowSoftToast(const std::wstring& body);
void OpenImageExternal(const ClipItem& item);
void CopyItemToClipboard(HWND hwnd, const ClipItem& item);

struct CardButtonState {
    bool infoHovered = false;
    bool copyHovered = false;
    bool pinHovered = false;
    bool cardHovered = false;
    float infoHoverProgress = 0.0f;
    float copyHoverProgress = 0.0f;
    float pinHoverProgress = 0.0f;
    float cardHoverProgress = 0.0f;
    float infoPressProgress = 0.0f;
    float copyPressProgress = 0.0f;
    float pinPressProgress = 0.0f;
    ULONGLONG copiedTimestamp = 0;
};
std::vector<CardButtonState> gCardButtonStates;

struct CardHit {
    RECT bounds{};
    RECT infoRect{};
    RECT pinRect{};
    RECT copyRect{};
    size_t historyIndex = 0;
};
std::vector<CardHit> gCardHits;

COLORREF HslToRgb(float h, float s, float l) {
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    s = max(0.0f, min(1.0f, s));
    l = max(0.0f, min(1.0f, l));
    float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::abs(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;
    float r = 0, g = 0, b = 0;
    if (h < 60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    int ir = static_cast<int>((r + m) * 255.0f + 0.5f);
    int ig = static_cast<int>((g + m) * 255.0f + 0.5f);
    int ib = static_cast<int>((b + m) * 255.0f + 0.5f);
    return RGB(max(0, min(255, ir)), max(0, min(255, ig)), max(0, min(255, ib)));
}

void RgbToHsl(COLORREF col, float& outH, float& outS, float& outL) {
    float r = GetRValue(col) / 255.0f;
    float g = GetGValue(col) / 255.0f;
    float b = GetBValue(col) / 255.0f;
    float cMax = max(r, max(g, b));
    float cMin = min(r, min(g, b));
    float delta = cMax - cMin;
    outL = (cMax + cMin) / 2.0f;
    if (delta == 0.0f) {
        outH = 0.0f;
        outS = 0.0f;
    } else {
        outS = outL > 0.5f ? delta / (2.0f - cMax - cMin) : delta / (cMax + cMin);
        if (cMax == r) {
            outH = fmodf((g - b) / delta, 6.0f);
        } else if (cMax == g) {
            outH = ((b - r) / delta) + 2.0f;
        } else {
            outH = ((r - g) / delta) + 4.0f;
        }
        outH *= 60.0f;
        if (outH < 0.0f) outH += 360.0f;
    }
}

bool TryParseColor(const std::wstring& rawStr, COLORREF& outColor) {
    if (rawStr.empty() || rawStr.size() > 128) return false;

    std::wstring s = rawStr;
    if (!s.empty() && s[0] == L'\xFEFF') s.erase(s.begin());

    auto isTrimChar = [](wchar_t c) {
        return iswspace(c) || c == L'"' || c == L'\'' || c == L'`' || c == L';' || c == L',';
    };
    while (!s.empty() && isTrimChar(s.front())) s.erase(s.begin());
    while (!s.empty() && isTrimChar(s.back())) s.pop_back();
    if (s.empty()) return false;

    std::wstring lowerPrefix = s;
    for (auto& c : lowerPrefix) c = towlower(c);

    static const wchar_t* prefixes[] = {
        L"background-color:", L"background:", L"border-color:", L"color:",
        L"fill:", L"stroke:", L"hex:", L"hex ", L"rgb:", L"rgb ", L"color="
    };
    for (const auto* pfx : prefixes) {
        if (lowerPrefix.rfind(pfx, 0) == 0) {
            s = s.substr(wcslen(pfx));
            while (!s.empty() && isTrimChar(s.front())) s.erase(s.begin());
            while (!s.empty() && isTrimChar(s.back())) s.pop_back();
            break;
        }
    }
    if (s.empty()) return false;

    std::wstring sClean = s;
    sClean.erase(std::remove_if(sClean.begin(), sClean.end(), iswspace), sClean.end());
    std::wstring sLower = sClean;
    for (auto& c : sLower) c = towlower(c);

    // 1. Functional rgb() / rgba()
    if (sLower.rfind(L"rgb(", 0) == 0 || sLower.rfind(L"rgba(", 0) == 0) {
        const size_t openParen = sLower.find(L'(');
        const size_t closeParen = sLower.rfind(L')');
        if (openParen != std::wstring::npos && closeParen != std::wstring::npos && closeParen > openParen) {
            std::wstring args = sLower.substr(openParen + 1, closeParen - openParen - 1);
            for (auto& ch : args) {
                if (ch == L',' || ch == L'/') ch = L' ';
            }
            float rF = 0, gF = 0, bF = 0;
            if (swscanf_s(args.c_str(), L"%f %f %f", &rF, &gF, &bF) == 3) {
                int r = max(0, min(255, static_cast<int>(rF)));
                int g = max(0, min(255, static_cast<int>(gF)));
                int b = max(0, min(255, static_cast<int>(bF)));
                outColor = RGB(r, g, b);
                return true;
            }
        }
    }

    // 2. Functional hsl() / hsla()
    if (sLower.rfind(L"hsl(", 0) == 0 || sLower.rfind(L"hsla(", 0) == 0) {
        const size_t openParen = sLower.find(L'(');
        const size_t closeParen = sLower.rfind(L')');
        if (openParen != std::wstring::npos && closeParen != std::wstring::npos && closeParen > openParen) {
            std::wstring args = sLower.substr(openParen + 1, closeParen - openParen - 1);
            for (auto& ch : args) {
                if (ch == L',' || ch == L'%' || ch == L'/') ch = L' ';
            }
            float h = 0, sVal = 0, lVal = 0;
            if (swscanf_s(args.c_str(), L"%f %f %f", &h, &sVal, &lVal) == 3) {
                outColor = HslToRgb(h, sVal / 100.0f, lVal / 100.0f);
                return true;
            }
        }
    }

    // 3. Hex formats: #RRGGBB, #RGB, #RRGGBBAA, #RGBA, 0xRRGGBB, or pure RRGGBB
    std::wstring hexStr = sClean;
    bool hasHash = false;
    if (!hexStr.empty() && hexStr[0] == L'#') {
        hasHash = true;
        hexStr.erase(0, 1);
    } else if (hexStr.rfind(L"0x", 0) == 0 || hexStr.rfind(L"0X", 0) == 0) {
        hasHash = true;
        hexStr.erase(0, 2);
    }

    auto isHexOnly = [](const std::wstring& str) {
        if (str.empty()) return false;
        for (wchar_t c : str) {
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) return false;
        }
        return true;
    };

    if (isHexOnly(hexStr)) {
        if (hexStr.size() == 6) {
            wchar_t* endPtr = nullptr;
            unsigned long val = wcstoul(hexStr.c_str(), &endPtr, 16);
            if (endPtr && *endPtr == L'\0') {
                int r = (val >> 16) & 0xFF;
                int g = (val >> 8) & 0xFF;
                int b = val & 0xFF;
                outColor = RGB(r, g, b);
                return true;
            }
        } else if (hexStr.size() == 3 && hasHash) {
            wchar_t* endPtr = nullptr;
            unsigned long val = wcstoul(hexStr.c_str(), &endPtr, 16);
            if (endPtr && *endPtr == L'\0') {
                int r = ((val >> 8) & 0xF) * 17;
                int g = ((val >> 4) & 0xF) * 17;
                int b = (val & 0xF) * 17;
                outColor = RGB(r, g, b);
                return true;
            }
        } else if (hexStr.size() == 8) {
            std::wstring first6 = hexStr.substr(0, 6);
            wchar_t* endPtr = nullptr;
            unsigned long val = wcstoul(first6.c_str(), &endPtr, 16);
            if (endPtr && *endPtr == L'\0') {
                int r = (val >> 16) & 0xFF;
                int g = (val >> 8) & 0xFF;
                int b = val & 0xFF;
                outColor = RGB(r, g, b);
                return true;
            }
        } else if (hexStr.size() == 4 && hasHash) {
            std::wstring first3 = hexStr.substr(0, 3);
            wchar_t* endPtr = nullptr;
            unsigned long val = wcstoul(first3.c_str(), &endPtr, 16);
            if (endPtr && *endPtr == L'\0') {
                int r = ((val >> 8) & 0xF) * 17;
                int g = ((val >> 4) & 0xF) * 17;
                int b = (val & 0xF) * 17;
                outColor = RGB(r, g, b);
                return true;
            }
        }
    }

    // 4. Named colors
    struct NamedColor {
        const wchar_t* name;
        COLORREF color;
    };
    static const NamedColor namedColors[] = {
        { L"black", RGB(0, 0, 0) },
        { L"white", RGB(255, 255, 255) },
        { L"red", RGB(255, 0, 0) },
        { L"green", RGB(0, 128, 0) },
        { L"lime", RGB(0, 255, 0) },
        { L"blue", RGB(0, 0, 255) },
        { L"yellow", RGB(255, 255, 0) },
        { L"cyan", RGB(0, 255, 255) },
        { L"aqua", RGB(0, 255, 255) },
        { L"magenta", RGB(255, 0, 255) },
        { L"fuchsia", RGB(255, 0, 255) },
        { L"orange", RGB(255, 165, 0) },
        { L"purple", RGB(128, 0, 128) },
        { L"pink", RGB(255, 192, 203) },
        { L"gray", RGB(128, 128, 128) },
        { L"grey", RGB(128, 128, 128) },
        { L"silver", RGB(192, 192, 192) },
        { L"navy", RGB(0, 0, 128) },
        { L"teal", RGB(0, 128, 128) },
        { L"maroon", RGB(128, 0, 0) },
        { L"olive", RGB(128, 128, 0) },
        { L"gold", RGB(255, 215, 0) },
        { L"coral", RGB(255, 127, 80) },
        { L"indigo", RGB(75, 0, 130) },
        { L"violet", RGB(238, 130, 238) },
        { L"brown", RGB(165, 42, 42) }
    };
    for (const auto& nc : namedColors) {
        if (sLower == nc.name) {
            outColor = nc.color;
            return true;
        }
    }

    return false;
}

bool IsCodeSnippet(const std::wstring& str) {
    if (str.size() < 12) return false;
    static const wchar_t* keywords[] = {
        L"function", L"const ", L"let ", L"var ", L"class ",
        L"import ", L"export ", L"return ", L"def ", L"public:",
        L"private:", L"protected:", L"#include", L"namespace ",
        L"void ", L"int ", L"bool ", L"std::", L"System.",
        L"=>", L"==", L"!=", L"/*", L"*/", L"//", L"async ", L"await "
    };
    int hits = 0;
    for (const auto* kw : keywords) {
        if (str.find(kw) != std::wstring::npos) {
            hits++;
            if (hits >= 2) return true;
        }
    }
    if ((str.find(L'{') != std::wstring::npos && str.find(L'}') != std::wstring::npos) ||
        (str.find(L";\n") != std::wstring::npos) || (str.find(L";\r\n") != std::wstring::npos)) {
        return true;
    }
    return false;
}

std::vector<size_t> GetFilteredIndices() {
    std::vector<size_t> res;
    if (gSearchQuery.empty()) {
        res.resize(gHistory.size());
        for (size_t i = 0; i < gHistory.size(); ++i) res[i] = i;
        return res;
    }
    std::wstring qLower = gSearchQuery;
    for (auto& c : qLower) c = towlower(c);

    for (size_t i = 0; i < gHistory.size(); ++i) {
        const auto& it = gHistory[i];
        std::wstring pLower = it.preview + L" " + it.rawContent;
        for (auto& c : pLower) c = towlower(c);
        std::wstring sLower = it.source;
        for (auto& c : sLower) c = towlower(c);
        std::wstring tLower = it.pageTitle;
        for (auto& c : tLower) c = towlower(c);

        if (pLower.find(qLower) != std::wstring::npos ||
            sLower.find(qLower) != std::wstring::npos ||
            tLower.find(qLower) != std::wstring::npos) {
            res.push_back(i);
        }
    }
    std::stable_partition(res.begin(), res.end(), [](size_t idx) { return gHistory[idx].pinned; });
    return res;
}

void ToggleItemPinned(size_t index) {
    if (index >= gHistory.size()) return;
    gHistory[index].pinned = !gHistory[index].pinned;
    if (gHistory[index].pinned) {
        ClipItem item = std::move(gHistory[index]);
        gHistory.erase(gHistory.begin() + index);
        gHistory.insert(gHistory.begin(), std::move(item));
        ShowSoftToast(L"Item fixado no topo!");
    } else {
        ClipItem item = std::move(gHistory[index]);
        gHistory.erase(gHistory.begin() + index);
        auto insertPos = std::find_if(gHistory.begin(), gHistory.end(), [](const ClipItem& it) { return !it.pinned; });
        gHistory.insert(insertPos, std::move(item));
        ShowSoftToast(L"Item desafixado.");
    }
    SaveHistoryToDisk();
}
bool gClearHovered = false;
float gClearHoverProgress = 0.0f;
float gCloseHoverProgress = 0.0f;
constexpr UINT_PTR kMainAnimTimer = 10;
constexpr UINT kMainAnimInterval = 14;

DWORD gOurClipboardSequence = 0;
DWORD gLastProcessedClipboardSequence = 0;
DWORD gPendingClipboardSequence = 0;
int gClipboardRetryCount = 0;
uint64_t gLastClipboardImageFingerprint = 0;
ULONGLONG gLastClipboardImageTick = 0;
bool gHasClipboardImageFingerprint = false;

HWND gActiveModalHwnd = nullptr;
int gTargetX = 0;
int gTargetY = 0;
int gAnimationStep = 0;
bool gIsClosing = false;
constexpr int kAnimationFrames = 22;
constexpr int kAnimationOffset = 56;

inline COLORREF LerpColor(COLORREF c1, COLORREF c2, float t) {
    t = max(0.0f, min(1.0f, t));
    int r = static_cast<int>(GetRValue(c1) + t * (GetRValue(c2) - GetRValue(c1)));
    int g = static_cast<int>(GetGValue(c1) + t * (GetGValue(c2) - GetGValue(c1)));
    int b = static_cast<int>(GetBValue(c1) + t * (GetBValue(c2) - GetBValue(c1)));
    return RGB(r, g, b);
}

void FillRoundedRect(HDC dc, RECT r, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_NULL, 0, color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
    DeleteObject(brush); DeleteObject(pen);
}

void DrawTextLine(HDC dc, const wchar_t* text, RECT r, HFONT font, COLORREF color, UINT flags) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ oldFont = SelectObject(dc, font);
    DrawTextW(dc, text, -1, &r, flags);
    SelectObject(dc, oldFont);
}

void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
    SelectObject(dc, oldPen); DeleteObject(pen);
}

void DrawCard(HDC dc, RECT r, const ClipItem& item, bool selected, const CardButtonState& btnState, RECT& outInfoR, RECT& outPinR, RECT& outCopyR) {
    COLORREF baseCardBg = selected ? kCardSelected : kCardBackground;
    COLORREF hoverCardBg = selected ? RGB(45, 58, 86) : RGB(39, 52, 78);
    COLORREF cardBg = LerpColor(baseCardBg, hoverCardBg, btnState.cardHoverProgress);
    FillRoundedRect(dc, r, 8, cardBg);

    if (selected || btnState.cardHoverProgress > 0.01f) {
        COLORREF borderCol = selected ? kSelectedBorder : LerpColor(cardBg, RGB(68, 88, 126), btnState.cardHoverProgress);
        HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, r.left, r.top, r.right, r.bottom, 8, 8);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
    }

    if (item.pinned) {
        FillRoundedRect(dc, {r.left + 2, r.top + 10, r.left + 5, r.bottom - 10}, 2, kAccent);
    }

    COLORREF parsedColor{};
    const bool isColor = !item.image && TryParseColor(item.rawContent, parsedColor);
    const bool isCode = !item.image && !isColor && IsCodeSnippet(item.rawContent);

    int thumbnailWidth = 0;
    if (item.image) {
        thumbnailWidth = 58;
        BITMAP bitmap{};
        GetObjectW(item.image.get(), sizeof(bitmap), &bitmap);
        HDC memoryDc = CreateCompatibleDC(dc);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, item.image.get());
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, r.left + 12, r.top + 12, 48, 48, memoryDc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
        SelectObject(memoryDc, oldBitmap); DeleteDC(memoryDc);

        HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(55, 68, 92));
        HGDIOBJ oldP = SelectObject(dc, thumbPen);
        HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, r.left + 11, r.top + 11, r.left + 61, r.top + 61);
        SelectObject(dc, oldB); SelectObject(dc, oldP); DeleteObject(thumbPen);
    } else if (isColor) {
        thumbnailWidth = 58;
        RECT colRect = {r.left + 12, r.top + 12, r.left + 60, r.top + 60};
        FillRoundedRect(dc, colRect, 6, parsedColor);

        int brightness = (GetRValue(parsedColor) * 299 + GetGValue(parsedColor) * 587 + GetBValue(parsedColor) * 114) / 1000;
        COLORREF borderCol = brightness > 140 ? RGB(50, 65, 90) : RGB(130, 155, 200);
        HPEN cPen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ oldP = SelectObject(dc, cPen);
        HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, colRect.left, colRect.top, colRect.right, colRect.bottom, 6, 6);
        SelectObject(dc, oldB); SelectObject(dc, oldP); DeleteObject(cPen);
    } else if (isCode) {
        thumbnailWidth = 44;
        RECT codeBadge = {r.left + 12, r.top + 18, r.left + 48, r.top + 54};
        FillRoundedRect(dc, codeBadge, 6, RGB(25, 34, 52));
        DrawTextLine(dc, L"</>", codeBadge, gSmallFont, kAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    RECT text = {r.left + 14 + thumbnailWidth, r.top + 8, r.right - 44, r.top + 48};
    HFONT fontToUse = isCode ? gCodeFont : gBodyFont;
    DrawTextLine(dc, item.preview.c_str(), text, fontToUse, kText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

    wchar_t timeText[16]{};
    swprintf_s(timeText, L"%02u:%02u", item.copiedAt.wHour, item.copiedAt.wMinute);
    std::wstring metadata = item.source + L"  |  " + timeText;
    if (isColor) {
        wchar_t hexBuf[32]{};
        swprintf_s(hexBuf, L"Cor (#%02X%02X%02X)  |  ", GetRValue(parsedColor), GetGValue(parsedColor), GetBValue(parsedColor));
        metadata = hexBuf + metadata;
    } else if (isCode) {
        metadata = L"Codigo  |  " + metadata;
    }
    if (item.pinned) metadata = L"[Fixado]  " + metadata;

    COLORREF metaColor = item.pinned ? kAccent : kMuted;
    DrawTextLine(dc, metadata.c_str(), {r.left + 14 + thumbnailWidth, r.bottom - 24, r.right - 44, r.bottom - 6}, gSmallFont, metaColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // --- 3 Buttons on the right (Info, Pin, Copy) ---
    outInfoR = {r.right - 35, r.top + 5, r.right - 7, r.top + 28};
    float infoActive = max(btnState.infoHoverProgress, btnState.infoPressProgress);
    if (infoActive > 0.01f) {
        COLORREF infoBg = LerpColor(cardBg, RGB(48, 62, 92), btnState.infoHoverProgress);
        if (btnState.infoPressProgress > 0.01f) infoBg = LerpColor(infoBg, RGB(35, 80, 140), btnState.infoPressProgress);
        FillRoundedRect(dc, outInfoR, 5, infoBg);
        HPEN bPen = CreatePen(PS_SOLID, 1, LerpColor(cardBg, RGB(70, 90, 130), infoActive));
        HGDIOBJ oP = SelectObject(dc, bPen); HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, outInfoR.left, outInfoR.top, outInfoR.right, outInfoR.bottom, 5, 5);
        SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(bPen);
    }
    DrawTextLine(dc, ClipTraceIcons::kInfo, outInfoR, gIconFont, LerpColor(kMuted, RGB(255, 255, 255), infoActive), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    outPinR = {r.right - 35, r.top + 30, r.right - 7, r.top + 53};
    float pinActive = max(btnState.pinHoverProgress, btnState.pinPressProgress);
    if (item.pinned || pinActive > 0.01f) {
        COLORREF pinBg = item.pinned ? RGB(26, 48, 80) : LerpColor(cardBg, RGB(48, 62, 92), btnState.pinHoverProgress);
        if (btnState.pinPressProgress > 0.01f) pinBg = LerpColor(pinBg, RGB(35, 80, 140), btnState.pinPressProgress);
        FillRoundedRect(dc, outPinR, 5, pinBg);
        COLORREF pinBorder = item.pinned ? kAccent : LerpColor(cardBg, RGB(70, 90, 130), pinActive);
        HPEN bPen = CreatePen(PS_SOLID, 1, pinBorder);
        HGDIOBJ oP = SelectObject(dc, bPen); HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, outPinR.left, outPinR.top, outPinR.right, outPinR.bottom, 5, 5);
        SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(bPen);
    }
    COLORREF pinColor = item.pinned ? kAccent : LerpColor(kMuted, RGB(255, 255, 255), pinActive);
    const wchar_t* pinIcon = item.pinned ? ClipTraceIcons::kUnpin : ClipTraceIcons::kPin;
    DrawTextLine(dc, pinIcon, outPinR, gIconFont, pinColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    outCopyR = {r.right - 35, r.top + 55, r.right - 7, r.top + 78};
    const bool isJustCopied = (btnState.copiedTimestamp != 0) && (GetTickCount64() - btnState.copiedTimestamp < 1000);
    float copyActive = isJustCopied ? 1.0f : max(btnState.copyHoverProgress, btnState.copyPressProgress);
    if (copyActive > 0.01f) {
        COLORREF copyBg = LerpColor(cardBg, RGB(48, 62, 92), btnState.copyHoverProgress);
        if (isJustCopied) copyBg = RGB(16, 124, 65);
        else if (btnState.copyPressProgress > 0.01f) copyBg = LerpColor(copyBg, kAccent, btnState.copyPressProgress);
        FillRoundedRect(dc, outCopyR, 5, copyBg);
        COLORREF copyBorder = isJustCopied ? RGB(32, 175, 95) : LerpColor(cardBg, RGB(70, 90, 130), copyActive);
        HPEN bPen = CreatePen(PS_SOLID, 1, copyBorder);
        HGDIOBJ oP = SelectObject(dc, bPen); HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, outCopyR.left, outCopyR.top, outCopyR.right, outCopyR.bottom, 5, 5);
        SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(bPen);
    }
    COLORREF copyColor = isJustCopied ? RGB(255, 255, 255) : LerpColor(kMuted, RGB(255, 255, 255), copyActive);
    const wchar_t* copyIcon = isJustCopied ? ClipTraceIcons::kCheckMark : ClipTraceIcons::kCopy;
    DrawTextLine(dc, copyIcon, outCopyR, gIconFont, copyColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

bool IsInside(POINT point, RECT rect) {
    return PtInRect(&rect, point) != 0;
}

std::wstring NormalizePreview(std::wstring text) {
    for (wchar_t& character : text)
        if (character == L'\r' || character == L'\n' || character == L'\t') character = L' ';
    while (text.find(L"  ") != std::wstring::npos) text.replace(text.find(L"  "), 2, L" ");
    if (text.size() > 110) text = text.substr(0, 107) + L"...";
    return text;
}

void AddClipboardItem(const ClipItem& input) {
    ClipItem item = input;
    if (item.preview.empty()) return;
    GetLocalTime(&item.copiedAt);
    const std::wstring preview = item.preview;
    if (preview.empty()) return;

    if (item.type == ClipItem::Type::Image && item.image && item.imageFile.empty()) {
        std::wstring appData = GetAppDataDirectory();
        if (!appData.empty()) {
            static UINT sImgCounter = 0;
            wchar_t fileName[64]{};
            swprintf_s(fileName, L"img_%llu_%u.bmp", GetTickCount64(), ++sImgCounter);
            std::wstring fullPath = appData + L"\\images\\" + fileName;
            if (SaveBitmapToFile(item.image.get(), fullPath)) {
                item.imageFile = fileName;
            }
        }
    }

    const auto existing = item.type == ClipItem::Type::Image ? gHistory.end() :
        std::find_if(gHistory.begin(), gHistory.end(), [&](const ClipItem& existingItem) {
            return existingItem.type == item.type && existingItem.rawContent == item.rawContent && existingItem.source == item.source;
        });
    bool wasPinned = false;
    if (existing != gHistory.end()) {
        wasPinned = existing->pinned;
        gHistory.erase(existing);
    }
    item.pinned = wasPinned;
    if (item.pinned) {
        gHistory.insert(gHistory.begin(), std::move(item));
    } else {
        auto insertPos = std::find_if(gHistory.begin(), gHistory.end(), [](const ClipItem& it) { return !it.pinned; });
        gHistory.insert(insertPos, std::move(item));
    }
    if (gHistory.size() > 50) {
        const auto removable = std::find_if(gHistory.rbegin(), gHistory.rend(), [](const ClipItem& item) { return !item.pinned; });
        if (removable != gHistory.rend()) {
            if (!removable->imageFile.empty()) {
                std::wstring appData = GetAppDataDirectory();
                if (!appData.empty()) {
                    DeleteFileW((appData + L"\\images\\" + removable->imageFile).c_str());
                }
            }
            gHistory.erase(std::next(removable).base());
        }
    }
    gSelectedIndex = 0;
    ApplyAutoCleanup();
    SaveHistoryToDisk();
}

std::wstring WindowTitle(HWND owner) {
    if (!owner) return L"";
    wchar_t title[512]{};
    GetWindowTextW(owner, title, static_cast<int>(std::size(title)));
    return title;
}

struct WindowInfo {
    HWND hwnd = nullptr;
    std::wstring processName;
    std::wstring exePath;
    std::wstring exeDir;
    std::wstring title;
    bool isBrowser = false;
    bool isPasswordManager = false;
};

inline bool IsTransientCaptureOrShell(const std::wstring& processName, HWND w) {
    std::wstring lower = processName;
    for (auto& ch : lower) ch = towlower(ch);

    if (lower == L"screenclippinghost" ||
        lower == L"snippingtool" ||
        lower == L"snippingtoolapp" ||
        lower == L"cliptrace" ||
        lower == L"shellexperiencehost" ||
        lower == L"searchhost" ||
        lower == L"textinputhost" ||
        lower == L"startmenuexperiencehost") {
        return true;
    }

    wchar_t cls[64]{};
    GetClassNameW(w, cls, 64);
    if (wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0) {
        return true;
    }
    return false;
}

WindowInfo GetWindowInfo(HWND w) {
    WindowInfo info;
    if (!w || !IsWindow(w)) return info;
    info.hwnd = w;
    info.title = WindowTitle(w);

    DWORD processId = 0;
    GetWindowThreadProcessId(w, &processId);
    if (processId == 0) return info;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process) {
        wchar_t path[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            info.exePath = path;
            const size_t slash = info.exePath.find_last_of(L"\\/");
            if (slash != std::wstring::npos) {
                info.exeDir = info.exePath.substr(0, slash);
                std::wstring fileName = info.exePath.substr(slash + 1);
                const size_t ext = fileName.find_last_of(L'.');
                info.processName = (ext != std::wstring::npos) ? fileName.substr(0, ext) : fileName;
            } else {
                info.processName = info.exePath;
            }
        }
        CloseHandle(process);
    }

    std::wstring lower = info.processName;
    for (auto& ch : lower) ch = towlower(ch);
    static const wchar_t* browsers[] = {
        L"chrome", L"msedge", L"firefox", L"brave", L"opera",
        L"vivaldi", L"arc", L"waterfox", L"zen", L"thorium"
    };
    for (const auto* b : browsers) {
        if (lower.find(b) != std::wstring::npos) {
            info.isBrowser = true;
            break;
        }
    }

    static const wchar_t* passwordManagers[] = {
        L"1password", L"bitwarden", L"keepass", L"keepassxc",
        L"dashlane", L"lastpass", L"enpass", L"roboform",
        L"nordpass", L"authpass"
    };
    for (const auto* p : passwordManagers) {
        if (lower.find(p) != std::wstring::npos) {
            info.isPasswordManager = true;
            break;
        }
    }
    return info;
}

HWND FindTargetWindow() {
    HWND owner = GetClipboardOwner();
    if (owner && IsWindow(owner)) {
        WindowInfo info = GetWindowInfo(owner);
        if (!IsTransientCaptureOrShell(info.processName, owner) && !info.title.empty()) {
            return owner;
        }
    }

    HWND fg = GetForegroundWindow();
    if (fg && IsWindow(fg)) {
        WindowInfo info = GetWindowInfo(fg);
        if (!IsTransientCaptureOrShell(info.processName, fg) && !info.title.empty()) {
            return fg;
        }
    }

    for (HWND w = GetTopWindow(nullptr); w != nullptr; w = GetWindow(w, GW_HWNDNEXT)) {
        if (!IsWindowVisible(w) || IsIconic(w)) continue;

        RECT r{};
        GetWindowRect(w, &r);
        if ((r.right - r.left) < 150 || (r.bottom - r.top) < 150) continue;

        LONG exStyle = GetWindowLongW(w, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) continue;

        WindowInfo info = GetWindowInfo(w);
        if (info.title.empty()) continue;
        if (IsTransientCaptureOrShell(info.processName, w)) continue;

        return w;
    }

    return owner ? owner : fg;
}

std::wstring ExtractBrowserTabTitle(const std::wstring& fullTitle, const std::wstring& processName) {
    if (fullTitle.empty()) return fullTitle;

    static const wchar_t* suffixes[] = {
        L" - Google Chrome",
        L" - Microsoft Edge",
        L" - Microsoft\x200B Edge",
        L" — Mozilla Firefox",
        L" - Mozilla Firefox",
        L" - Brave",
        L" - Opera",
        L" - Vivaldi"
    };
    for (const auto* suf : suffixes) {
        size_t pos = fullTitle.rfind(suf);
        if (pos != std::wstring::npos && pos > 0) {
            return fullTitle.substr(0, pos);
        }
    }

    size_t lastDash = fullTitle.rfind(L" - ");
    if (lastDash != std::wstring::npos && lastDash > 0) {
        std::wstring after = fullTitle.substr(lastDash + 3);
        for (auto& c : after) c = towlower(c);
        std::wstring proc = processName;
        for (auto& c : proc) c = towlower(c);
        if (proc.find(after) != std::wstring::npos || after.find(proc) != std::wstring::npos) {
            return fullTitle.substr(0, lastDash);
        }
    }
    return fullTitle;
}

std::wstring GetBrowserUrlFromWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return L"";

    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needCoUninit = SUCCEEDED(hrCo);

    IUIAutomation* automation = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IUIAutomation, reinterpret_cast<void**>(&automation));
    if (FAILED(hr) || !automation) {
        if (needCoUninit) CoUninitialize();
        return L"";
    }

    std::wstring foundUrl;
    IUIAutomationElement* root = nullptr;
    hr = automation->ElementFromHandle(hwnd, &root);
    if (SUCCEEDED(hr) && root) {
        VARIANT varType;
        VariantInit(&varType);
        varType.vt = VT_I4;
        varType.lVal = UIA_EditControlTypeId;

        IUIAutomationCondition* condEdit = nullptr;
        automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varType, &condEdit);

        if (condEdit) {
            IUIAutomationElement* editElement = nullptr;
            hr = root->FindFirst(TreeScope_Descendants, condEdit, &editElement);
            if (SUCCEEDED(hr) && editElement) {
                IUIAutomationValuePattern* valPattern = nullptr;
                if (SUCCEEDED(editElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern, reinterpret_cast<void**>(&valPattern))) && valPattern) {
                    BSTR bstrVal = nullptr;
                    if (SUCCEEDED(valPattern->get_CurrentValue(&bstrVal)) && bstrVal) {
                        foundUrl = bstrVal;
                        SysFreeString(bstrVal);
                    }
                    valPattern->Release();
                }

                if (foundUrl.empty()) {
                    BSTR bstrName = nullptr;
                    if (SUCCEEDED(editElement->get_CurrentName(&bstrName)) && bstrName) {
                        foundUrl = bstrName;
                        SysFreeString(bstrName);
                    }
                }
                editElement->Release();
            }
            condEdit->Release();
        }
        root->Release();
    }
    automation->Release();
    if (needCoUninit) CoUninitialize();

    if (!foundUrl.empty()) {
        if (foundUrl.rfind(L"http://", 0) == 0 || foundUrl.rfind(L"https://", 0) == 0) {
            return foundUrl;
        }
        if (foundUrl.find(L'.') != std::wstring::npos && foundUrl.find(L' ') == std::wstring::npos) {
            return L"https://" + foundUrl;
        }
    }
    return L"";
}

std::wstring ProgramNameFromWindow(HWND owner) {
    WindowInfo info = GetWindowInfo(owner);
    return info.processName.empty() ? L"outro aplicativo" : info.processName;
}

std::wstring UrlFromHtmlClipboard() {
    const UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    if (!IsClipboardFormatAvailable(htmlFormat)) return L"";
    HGLOBAL data = GetClipboardData(htmlFormat);
    const char* html = data ? static_cast<const char*>(GlobalLock(data)) : nullptr;
    if (!html) return L"";
    const char* marker = strstr(html, "SourceURL:");
    if (!marker) { GlobalUnlock(data); return L""; }
    marker += 10;
    const char* end = strstr(marker, "\r\n");
    std::string url(marker, end ? end : marker + strlen(marker));
    GlobalUnlock(data);
    return std::wstring(url.begin(), url.end());
}

std::wstring SiteFromUrl(const std::wstring& url) {
    const size_t protocol = url.find(L"://");
    const size_t hostStart = protocol == std::wstring::npos ? 0 : protocol + 3;
    const size_t hostEnd = url.find(L'/', hostStart);
    std::wstring host = url.substr(hostStart, hostEnd - hostStart);
    if (host.rfind(L"www.", 0) == 0) host.erase(0, 4);
    return host;
}

#ifndef NIIF_USER
#define NIIF_USER 0x00000004
#endif
#ifndef NIIF_LARGE_ICON
#define NIIF_LARGE_ICON 0x00000020
#endif

HICON CreateColorIcon(COLORREF color, int size = 48) {
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hColorBmp = CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hColorBmp || !bits) {
        if (hColorBmp) DeleteObject(hColorBmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }
    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDc, hColorBmp);
    memset(bits, 0, size * size * 4);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        const int pad = 2;
        const int r = 8;
        const int d = r * 2;
        const int w = size - pad * 2;
        const int h = size - pad * 2;
        const int x = pad;
        const int y = pad;

        Gdiplus::GraphicsPath path;
        path.AddArc(x, y, d, d, 180, 90);
        path.AddArc(x + w - d, y, d, d, 270, 90);
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
        path.CloseFigure();

        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
        g.FillPath(&brush, &path);

        int brightness = (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
        Gdiplus::Color borderCol = brightness > 140 ? Gdiplus::Color(255, 60, 75, 100) : Gdiplus::Color(255, 190, 210, 245);
        Gdiplus::Pen pen(borderCol, 2.0f);
        g.DrawPath(&pen, &path);
    }

    SelectObject(memDc, hOldBmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    const int maskStride = ((size + 31) / 32) * 4;
    std::vector<BYTE> maskBits(maskStride * size, 0);
    HBITMAP hMaskBmp = CreateBitmap(size, size, 1, 1, maskBits.data());

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hColorBmp;
    ii.hbmMask = hMaskBmp;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);
    return hIcon;
}

HICON CreateImageThumbnailIcon(HBITMAP hSrcBitmap, int size = 48) {
    if (!hSrcBitmap) return nullptr;
    BITMAP bm{};
    if (!GetObjectW(hSrcBitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return nullptr;

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hColorBmp = CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hColorBmp || !bits) {
        if (hColorBmp) DeleteObject(hColorBmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }
    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDc, hColorBmp);
    memset(bits, 0, size * size * 4);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

        const int pad = 2;
        const int r = 8;
        const int d = r * 2;
        const int w = size - pad * 2;
        const int h = size - pad * 2;
        const int x = pad;
        const int y = pad;

        Gdiplus::GraphicsPath path;
        path.AddArc(x, y, d, d, 180, 90);
        path.AddArc(x + w - d, y, d, d, 270, 90);
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
        path.CloseFigure();

        g.SetClip(&path);

        Gdiplus::Bitmap gdiBmp(hSrcBitmap, nullptr);
        float scale = max(static_cast<float>(w) / bm.bmWidth, static_cast<float>(h) / bm.bmHeight);
        int drawW = static_cast<int>(bm.bmWidth * scale);
        int drawH = static_cast<int>(bm.bmHeight * scale);
        int drawX = x + (w - drawW) / 2;
        int drawY = y + (h - drawH) / 2;
        g.DrawImage(&gdiBmp, drawX, drawY, drawW, drawH);

        g.ResetClip();
        Gdiplus::Pen pen(Gdiplus::Color(255, 65, 85, 120), 2.0f);
        g.DrawPath(&pen, &path);
    }

    SelectObject(memDc, hOldBmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    const int maskStride = ((size + 31) / 32) * 4;
    std::vector<BYTE> maskBits(maskStride * size, 0);
    HBITMAP hMaskBmp = CreateBitmap(size, size, 1, 1, maskBits.data());

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hColorBmp;
    ii.hbmMask = hMaskBmp;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);
    return hIcon;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    auto pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
    if (pImageCodecInfo == nullptr) return -1;
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return static_cast<int>(j);
        }
    }
    free(pImageCodecInfo);
    return -1;
}

inline CLSID GetPngEncoderClsid() {
    CLSID clsid = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    GetEncoderClsid(L"image/png", &clsid);
    return clsid;
}

bool SaveBitmapToPngFile(HBITMAP hBitmap, const std::wstring& filePath) {
    if (!hBitmap) return false;
    CLSID pngClsid = GetPngEncoderClsid();
    Gdiplus::Bitmap bmp(hBitmap, nullptr);
    if (bmp.GetLastStatus() != Gdiplus::Ok) return false;
    return bmp.Save(filePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

bool SaveColorSwatchToPng(COLORREF color, const std::wstring& filePath, int size = 160) {
    CLSID pngClsid = GetPngEncoderClsid();
    Gdiplus::Bitmap bmp(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
        g.FillRectangle(&brush, 0, 0, size, size);

        Gdiplus::Pen pen(Gdiplus::Color(80, 255, 255, 255), 2.0f);
        g.DrawRectangle(&pen, 1, 1, size - 2, size - 2);
    }
    return bmp.Save(filePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

bool SaveIconToPngFile(HICON hIcon, const std::wstring& filePath, int size = 64) {
    if (!hIcon) return false;
    CLSID pngClsid = GetPngEncoderClsid();
    std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromHICON(hIcon));
    if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) {
        return bmp->Save(filePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
    }

    Gdiplus::Bitmap fallbackBmp(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&fallbackBmp);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        HDC hdc = g.GetHDC();
        DrawIconEx(hdc, 0, 0, hIcon, size, size, 0, nullptr, DI_NORMAL);
        g.ReleaseHDC(hdc);
    }
    return fallbackBmp.Save(filePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

std::wstring PathToFileUri(const std::wstring& path) {
    std::wstring uri = L"file:///";
    for (wchar_t c : path) {
        if (c == L'\\') uri += L'/';
        else uri += c;
    }
    return uri;
}

std::wstring EscapeXml(const std::wstring& text) {
    std::wstring result;
    result.reserve(text.size() + 16);
    for (wchar_t c : text) {
        switch (c) {
        case L'&': result += L"&amp;"; break;
        case L'<': result += L"&lt;"; break;
        case L'>': result += L"&gt;"; break;
        case L'"': result += L"&quot;"; break;
        case L'\'': result += L"&apos;"; break;
        default: result += c; break;
        }
    }
    return result;
}

bool ShowNativeToast(const std::wstring& xmlString) {
    try {
        winrt::Windows::Data::Xml::Dom::XmlDocument doc;
        doc.LoadXml(xmlString);
        winrt::Windows::UI::Notifications::ToastNotification toast(doc);
        auto notifier = winrt::Windows::UI::Notifications::ToastNotificationManager::CreateToastNotifier(L"palmeidev.ClipTrace");
        notifier.Show(toast);
        return true;
    } catch (...) {
        return false;
    }
}

void EnsureStartMenuShortcut() {
    wchar_t programsPath[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programsPath))) return;
    std::wstring shortcutPath = std::wstring(programsPath) + L"\\ClipTrace.lnk";

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(exePath);
        psl->SetIconLocation(exePath, 0);
        psl->SetDescription(L"ClipTrace");

        IPropertyStore* pps = nullptr;
        hr = psl->QueryInterface(IID_IPropertyStore, reinterpret_cast<void**>(&pps));
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            pv.vt = VT_LPWSTR;
            pv.pwszVal = const_cast<PWSTR>(L"palmeidev.ClipTrace");
            pps->SetValue(PKEY_AppUserModel_ID, pv);
            pps->Commit();
            pps->Release();
        }

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
        if (SUCCEEDED(hr)) {
            ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
}

void RegisterAppUserModelId(HICON hAppIcon) {
    SetCurrentProcessExplicitAppUserModelID(L"palmeidev.ClipTrace");

    std::wstring appData = GetAppDataDirectory();
    std::wstring iconPngPath = appData + L"\\app_icon.png";
    if (hAppIcon) {
        SaveIconToPngFile(hAppIcon, iconPngPath, 64);
    }

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
        std::wstring localIco = exeDir + L"\\ClipTrace.ico";
        std::wstring destIco = appData + L"\\ClipTrace.ico";
        if (GetFileAttributesW(localIco.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(localIco.c_str(), destIco.c_str(), FALSE);
        }
    }

    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\AppUserModelId\\palmeidev.ClipTrace", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* name = L"ClipTrace";
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, reinterpret_cast<const BYTE*>(name), static_cast<DWORD>((wcslen(name) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"IconUri", 0, REG_SZ, reinterpret_cast<const BYTE*>(iconPngPath.c_str()), static_cast<DWORD>((iconPngPath.size() + 1) * sizeof(wchar_t)));
        DWORD show = 1;
        RegSetValueExW(hKey, L"ShowInSettings", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&show), sizeof(show));
        RegCloseKey(hKey);
    }

    EnsureStartMenuShortcut();
}

void ShowCopiedNotification(const ClipItem& item, const std::wstring& source) {
    static std::wstring sLastNotified;
    static ULONGLONG sLastNotifyTime = 0;
    static HICON sLastCustomIcon = nullptr;
    const ULONGLONG now = GetTickCount64();

    std::wstring title = L"ClipTrace";
    std::wstring body;
    HICON hNotifIcon = nullptr;
    bool isCustomCreatedIcon = false;

    COLORREF notifCol{};
    const bool isColor = !item.image && TryParseColor(item.rawContent, notifCol);

    std::wstring appData = GetAppDataDirectory();
    std::wstring appIconPath = appData + L"\\app_icon.png";
    std::wstring xml;

    if (item.type == ClipItem::Type::Image || item.image) {
        title = L"ClipTrace - Imagem copiada";
        const std::wstring label = !item.pageTitle.empty() ? item.pageTitle : item.source;
        const std::wstring origin = source.empty() ? label : source;
        body = L"Captura salva!\nOrigem: " + origin;

        static int sHeroIdx = 0;
        sHeroIdx = (sHeroIdx + 1) % 5;
        std::wstring heroPath = appData + L"\\images\\toast_hero_" + std::to_wstring(sHeroIdx) + L".png";
        bool imgSaved = false;

        if (item.image) {
            imgSaved = SaveBitmapToPngFile(item.image.get(), heroPath);
            hNotifIcon = CreateImageThumbnailIcon(item.image.get(), 48);
            isCustomCreatedIcon = true;
        } else if (!item.imageFile.empty()) {
            std::wstring fullPath = appData + L"\\images\\" + item.imageFile;
            Gdiplus::Bitmap fileBmp(fullPath.c_str());
            if (fileBmp.GetLastStatus() == Gdiplus::Ok) {
                CLSID pngClsid = GetPngEncoderClsid();
                imgSaved = (fileBmp.Save(heroPath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok);
            }
        }

        xml = L"<toast><visual><binding template=\"ToastGeneric\">";
        if (imgSaved) {
            xml += L"<image placement=\"hero\" src=\"" + EscapeXml(PathToFileUri(heroPath)) + L"\"/>";
        }
        xml += L"<text>" + EscapeXml(title) + L"</text>";
        xml += L"<text>Captura salva na \x00E1rea de transfer\x00EAncia</text>";
        if (!origin.empty()) {
            xml += L"<text>" + EscapeXml(L"Origem: " + origin) + L"</text>";
        }
        xml += L"</binding></visual></toast>";

    } else if (isColor) {
        title = L"ClipTrace - Cor copiada";
        wchar_t hexBuf[64]{};
        swprintf_s(hexBuf, L"#%02X%02X%02X",
            GetRValue(notifCol), GetGValue(notifCol), GetBValue(notifCol));
        wchar_t rgbBuf[64]{};
        swprintf_s(rgbBuf, L"RGB(%d, %d, %d)",
            GetRValue(notifCol), GetGValue(notifCol), GetBValue(notifCol));

        body = std::wstring(hexBuf) + L"  (" + rgbBuf + L")\nOrigem: " + source;
        hNotifIcon = CreateColorIcon(notifCol, 48);
        isCustomCreatedIcon = true;

        static int sColorIdx = 0;
        sColorIdx = (sColorIdx + 1) % 5;
        std::wstring colorPath = appData + L"\\images\\toast_color_" + std::to_wstring(sColorIdx) + L".png";
        SaveColorSwatchToPng(notifCol, colorPath, 160);

        xml = L"<toast><visual><binding template=\"ToastGeneric\">";
        xml += L"<image placement=\"appLogoOverride\" hint-crop=\"none\" src=\"" + EscapeXml(PathToFileUri(colorPath)) + L"\"/>";
        xml += L"<text>" + EscapeXml(title) + L"</text>";
        xml += L"<text>" + EscapeXml(hexBuf) + L"  -  " + EscapeXml(rgbBuf) + L"</text>";
        if (!source.empty()) {
            xml += L"<text>" + EscapeXml(L"Origem: " + source) + L"</text>";
        }
        xml += L"</binding></visual></toast>";

    } else if (item.type == ClipItem::Type::Link) {
        title = L"ClipTrace - Link copiado";
        body = item.preview + L"\nOrigem: " + source;

        xml = L"<toast><visual><binding template=\"ToastGeneric\">";
        if (GetFileAttributesW(appIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            xml += L"<image placement=\"appLogoOverride\" hint-crop=\"circle\" src=\"" + EscapeXml(PathToFileUri(appIconPath)) + L"\"/>";
        }
        xml += L"<text>" + EscapeXml(title) + L"</text>";
        xml += L"<text>" + EscapeXml(item.preview) + L"</text>";
        if (!source.empty()) {
            xml += L"<text>" + EscapeXml(L"Origem: " + source) + L"</text>";
        }
        xml += L"</binding></visual></toast>";

    } else if (item.type == ClipItem::Type::File) {
        title = L"ClipTrace - Arquivo copiado";
        body = item.preview + L"\nOrigem: " + source;

        xml = L"<toast><visual><binding template=\"ToastGeneric\">";
        if (GetFileAttributesW(appIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            xml += L"<image placement=\"appLogoOverride\" hint-crop=\"circle\" src=\"" + EscapeXml(PathToFileUri(appIconPath)) + L"\"/>";
        }
        xml += L"<text>" + EscapeXml(title) + L"</text>";
        xml += L"<text>" + EscapeXml(item.preview) + L"</text>";
        if (!source.empty()) {
            xml += L"<text>" + EscapeXml(L"Origem: " + source) + L"</text>";
        }
        xml += L"</binding></visual></toast>";

    } else {
        title = L"ClipTrace - Texto copiado";
        body = item.preview + L"\nOrigem: " + source;

        xml = L"<toast><visual><binding template=\"ToastGeneric\">";
        if (GetFileAttributesW(appIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            xml += L"<image placement=\"appLogoOverride\" hint-crop=\"circle\" src=\"" + EscapeXml(PathToFileUri(appIconPath)) + L"\"/>";
        }
        xml += L"<text>" + EscapeXml(title) + L"</text>";
        xml += L"<text>" + EscapeXml(item.preview) + L"</text>";
        if (!source.empty()) {
            xml += L"<text>" + EscapeXml(L"Origem: " + source) + L"</text>";
        }
        xml += L"</binding></visual></toast>";
    }

    if (body == sLastNotified && (now - sLastNotifyTime < 2000)) {
        if (isCustomCreatedIcon && hNotifIcon) DestroyIcon(hNotifIcon);
        return;
    }
    if (now - sLastNotifyTime < 300) {
        if (isCustomCreatedIcon && hNotifIcon) DestroyIcon(hNotifIcon);
        return;
    }
    sLastNotified = body;
    sLastNotifyTime = now;

    if (!xml.empty() && ShowNativeToast(xml)) {
        if (isCustomCreatedIcon && hNotifIcon) DestroyIcon(hNotifIcon);
        return;
    }

    if (gTrayIcon.hWnd != nullptr) {
        if (!hNotifIcon) {
            hNotifIcon = gLargeAppIcon ? gLargeAppIcon : gTrayIcon.hIcon;
        }
        if (sLastCustomIcon) {
            DestroyIcon(sLastCustomIcon);
            sLastCustomIcon = nullptr;
        }
        if (isCustomCreatedIcon) {
            sLastCustomIcon = hNotifIcon;
        }

        NOTIFYICONDATAW notification = gTrayIcon;
        notification.uFlags = NIF_INFO | NIF_ICON;
        if (body.size() >= std::size(notification.szInfo)) body.resize(std::size(notification.szInfo) - 1);
        wcsncpy_s(notification.szInfo, body.c_str(), _TRUNCATE);
        wcsncpy_s(notification.szInfoTitle, title.c_str(), _TRUNCATE);
        notification.hBalloonIcon = hNotifIcon;
        notification.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
        gUpdateBalloonActive = false;
        Shell_NotifyIconW(NIM_MODIFY, &notification);
    }
}

void ShowSoftToast(const std::wstring& body) {
    static std::wstring sLastToast;
    static ULONGLONG sLastToastTime = 0;
    const ULONGLONG now = GetTickCount64();

    if (body == sLastToast && (now - sLastToastTime < 1500)) {
        return;
    }
    if (now - sLastToastTime < 200) {
        return;
    }
    sLastToast = body;
    sLastToastTime = now;

    std::wstring appData = GetAppDataDirectory();
    std::wstring appIconPath = appData + L"\\app_icon.png";

    std::wstring xml = L"<toast><visual><binding template=\"ToastGeneric\">";
    if (GetFileAttributesW(appIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        xml += L"<image placement=\"appLogoOverride\" hint-crop=\"circle\" src=\"" + EscapeXml(PathToFileUri(appIconPath)) + L"\"/>";
    }
    xml += L"<text>ClipTrace</text>";
    xml += L"<text>" + EscapeXml(body) + L"</text>";
    xml += L"</binding></visual></toast>";

    if (ShowNativeToast(xml)) {
        return;
    }

    if (gTrayIcon.hWnd != nullptr) {
        NOTIFYICONDATAW notification = gTrayIcon;
        notification.uFlags = NIF_INFO;
        wcsncpy_s(notification.szInfo, body.c_str(), _TRUNCATE);
        wcsncpy_s(notification.szInfoTitle, L"ClipTrace", _TRUNCATE);
        notification.hBalloonIcon = gLargeAppIcon ? gLargeAppIcon : gTrayIcon.hIcon;
        notification.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
        gUpdateBalloonActive = false;
        Shell_NotifyIconW(NIM_MODIFY, &notification);
    }
}

void ShowUpdateNotification(HWND hwnd) {
    if (!gAvailableUpdate) return;
    const std::wstring message = L"Versão " + gAvailableUpdate->tag + L" disponível. Clique para baixar e instalar.";
    try {
        winrt::Windows::Data::Xml::Dom::XmlDocument doc;
        doc.LoadXml(L"<toast duration=\"long\"><visual><binding template=\"ToastGeneric\"><text>Atualização do ClipTrace</text><text>" +
            EscapeXml(message) + L"</text></binding></visual></toast>");
        gUpdateToast = winrt::Windows::UI::Notifications::ToastNotification(doc);
        gUpdateToast.Activated([hwnd](auto const&, auto const&) {
            PostMessageW(hwnd, WM_INSTALL_UPDATE, 0, 0);
        });
        auto notifier = winrt::Windows::UI::Notifications::ToastNotificationManager::CreateToastNotifier(L"palmeidev.ClipTrace");
        notifier.Show(gUpdateToast);
        gUpdateBalloonActive = false;
    } catch (...) {
        NOTIFYICONDATAW notification = gTrayIcon;
        notification.uFlags = NIF_INFO;
        wcsncpy_s(notification.szInfoTitle, L"Atualização do ClipTrace", _TRUNCATE);
        wcsncpy_s(notification.szInfo, message.c_str(), _TRUNCATE);
        notification.dwInfoFlags = NIIF_INFO;
        gUpdateBalloonActive = Shell_NotifyIconW(NIM_MODIFY, &notification) != FALSE;
    }
}

const wchar_t* TypeName(ClipItem::Type type) {
    switch (type) {
    case ClipItem::Type::Text: return L"Texto";
    case ClipItem::Type::Link: return L"Link";
    case ClipItem::Type::Image: return L"Imagem ou captura";
    case ClipItem::Type::File: return L"Arquivo";
    case ClipItem::Type::Color: return L"Cor";
    default: return L"Outro";
    }
}

HGLOBAL CreateDIBFromHBitmap(HBITMAP hBitmap) {
    if (!hBitmap) return nullptr;
    BITMAP bm{};
    if (!GetObjectW(hBitmap, sizeof(bm), &bm)) return nullptr;

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    DWORD rowBytes = ((bm.bmWidth * 32 + 31) / 32) * 4;
    DWORD imageSize = rowBytes * bm.bmHeight;
    bi.biSizeImage = imageSize;

    DWORD totalSize = sizeof(BITMAPINFOHEADER) + imageSize;
    HGLOBAL hGlobal = GlobalAlloc(GHND, totalSize);
    if (!hGlobal) return nullptr;

    void* pMem = GlobalLock(hGlobal);
    if (!pMem) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    memcpy(pMem, &bi, sizeof(BITMAPINFOHEADER));
    BYTE* pBits = static_cast<BYTE*>(pMem) + sizeof(BITMAPINFOHEADER);

    HDC screenDc = GetDC(nullptr);
    GetDIBits(screenDc, hBitmap, 0, bm.bmHeight, pBits, reinterpret_cast<BITMAPINFO*>(pMem), DIB_RGB_COLORS);
    ReleaseDC(nullptr, screenDc);

    GlobalUnlock(hGlobal);
    return hGlobal;
}

HBITMAP CreateHBitmapFromDIB(HGLOBAL hDib) {
    if (!hDib) return nullptr;
    void* pMem = GlobalLock(hDib);
    if (!pMem) return nullptr;

    auto* bi = static_cast<BITMAPINFOHEADER*>(pMem);
    int width = bi->biWidth;
    int height = abs(bi->biHeight);
    int numColors = 0;
    if (bi->biBitCount <= 8) {
        numColors = bi->biClrUsed ? bi->biClrUsed : (1 << bi->biBitCount);
    }
    BYTE* pBits = static_cast<BYTE*>(pMem) + bi->biSize + (numColors * sizeof(RGBQUAD));
    if (bi->biCompression == BI_BITFIELDS) {
        pBits = static_cast<BYTE*>(pMem) + bi->biSize + 12;
    }

    HDC screenDc = GetDC(nullptr);
    void* pDIBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDc, reinterpret_cast<BITMAPINFO*>(bi), DIB_RGB_COLORS, &pDIBits, nullptr, 0);
    if (hBitmap && pDIBits) {
        DWORD imgSize = bi->biSizeImage ? bi->biSizeImage : (((width * bi->biBitCount + 31) / 32) * 4 * height);
        memcpy(pDIBits, pBits, imgSize);
    }
    ReleaseDC(nullptr, screenDc);
    GlobalUnlock(hDib);
    return hBitmap;
}

bool SaveBitmapToFile(HBITMAP hBitmap, const std::wstring& filePath) {
    if (!hBitmap || filePath.empty()) return false;
    BITMAP bm{};
    if (!GetObjectW(hBitmap, sizeof(bm), &bm)) return false;

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // 'BM'
    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = bm.bmWidth;
    bih.biHeight = bm.bmHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    DWORD rowSize = ((bm.bmWidth * 32 + 31) / 32) * 4;
    DWORD imageSize = rowSize * bm.bmHeight;
    bih.biSizeImage = imageSize;

    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    std::vector<BYTE> bits(imageSize);
    HDC screenDc = GetDC(nullptr);
    const int scanlines = GetDIBits(screenDc, hBitmap, 0, bm.bmHeight, bits.data(), reinterpret_cast<BITMAPINFO*>(&bih), DIB_RGB_COLORS);
    ReleaseDC(nullptr, screenDc);
    if (scanlines != bm.bmHeight) return false;

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    const bool saved = WriteFile(hFile, &bfh, sizeof(bfh), &written, nullptr) && written == sizeof(bfh) &&
        WriteFile(hFile, &bih, sizeof(bih), &written, nullptr) && written == sizeof(bih) &&
        WriteFile(hFile, bits.data(), imageSize, &written, nullptr) && written == imageSize &&
        FlushFileBuffers(hFile);
    CloseHandle(hFile);
    if (!saved) DeleteFileW(filePath.c_str());
    return saved;
}

bool GetBitmapFingerprint(HBITMAP bitmap, uint64_t& fingerprint) {
    BITMAP details{};
    if (!bitmap || !GetObjectW(bitmap, sizeof(details), &details) || details.bmWidth <= 0 || details.bmHeight <= 0) return false;
    const size_t byteCount = static_cast<size_t>(details.bmWidth) * static_cast<size_t>(details.bmHeight) * 4;
    if (byteCount > 512ULL * 1024 * 1024) return false;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = details.bmWidth;
    info.bmiHeader.biHeight = details.bmHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<BYTE> pixels(byteCount);
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    const int lines = GetDIBits(dc, bitmap, 0, details.bmHeight, pixels.data(), &info, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines != details.bmHeight) return false;

    uint64_t hash = 14695981039346656037ULL;
    for (BYTE value : pixels) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    hash ^= static_cast<uint32_t>(details.bmWidth);
    hash *= 1099511628211ULL;
    hash ^= static_cast<uint32_t>(details.bmHeight);
    fingerprint = hash;
    return true;
}

bool IsRepeatedClipboardImage(HBITMAP bitmap, ULONGLONG now) {
    uint64_t fingerprint = 0;
    if (!GetBitmapFingerprint(bitmap, fingerprint)) return false;
    const bool repeated = gHasClipboardImageFingerprint &&
        now - gLastClipboardImageTick <= 350 && fingerprint == gLastClipboardImageFingerprint;
    gLastClipboardImageFingerprint = fingerprint;
    gLastClipboardImageTick = now;
    gHasClipboardImageFingerprint = true;
    return repeated;
}

bool SaveBitmapToTempFile(HBITMAP hBitmap, std::wstring& outPath) {
    wchar_t tempDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDir);
    static UINT sCounter = 0;
    wchar_t filePath[MAX_PATH]{};
    swprintf_s(filePath, L"%sClipTrace_%llu_%u.bmp", tempDir, GetTickCount64(), ++sCounter);
    if (SaveBitmapToFile(hBitmap, filePath)) {
        outPath = filePath;
        return true;
    }
    return false;
}

std::wstring GetAppDataDirectory() {
    wchar_t appData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
        return L"";
    }
    std::wstring dir = std::wstring(appData) + L"\\ClipTrace";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring imgDir = dir + L"\\images";
    CreateDirectoryW(imgDir.c_str(), nullptr);
    return dir;
}

void OpenImageExternal(const ClipItem& item) {
    std::wstring appData = GetAppDataDirectory();
    if (!appData.empty() && !item.imageFile.empty()) {
        std::wstring fullPath = appData + L"\\images\\" + item.imageFile;
        DWORD attr = GetFileAttributesW(fullPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            ShowSoftToast(L"Abrindo imagem no visualizador...");
            ShellExecuteW(nullptr, L"open", fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
    }
    if (item.image) {
        std::wstring path;
        if (SaveBitmapToTempFile(item.image.get(), path)) {
            ShowSoftToast(L"Abrindo imagem no visualizador...");
            ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
    }
    ShowSoftToast(L"Não foi possível abrir a imagem.");
}

bool WriteStorageString(HANDLE hFile, const std::wstring& str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    DWORD written = 0;
    if (!WriteFile(hFile, &len, sizeof(len), &written, nullptr) || written != sizeof(len)) return false;
    if (len > 0) {
        if (!WriteFile(hFile, str.data(), len * sizeof(wchar_t), &written, nullptr) || written != len * sizeof(wchar_t)) return false;
    }
    return true;
}

bool ReadStorageString(HANDLE hFile, std::wstring& str) {
    uint32_t len = 0;
    DWORD read = 0;
    if (!ReadFile(hFile, &len, sizeof(len), &read, nullptr) || read != sizeof(len)) return false;
    if (len > 2000000) return false;
    str.resize(len);
    if (len > 0) {
        if (!ReadFile(hFile, &str[0], len * sizeof(wchar_t), &read, nullptr) || read != len * sizeof(wchar_t)) return false;
    }
    return true;
}

void SaveHistoryToDisk() {
    std::wstring appData = GetAppDataDirectory();
    if (appData.empty()) return;
    std::wstring tmpPath = appData + L"\\history.tmp";
    std::wstring datPath = appData + L"\\history.dat";

    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    const uint32_t kMagic = 0x52544C43; // 'CLTR'
    const uint32_t kVersion = 1;
    uint32_t count = static_cast<uint32_t>(gHistory.size());

    bool saved = WriteFile(hFile, &kMagic, sizeof(kMagic), &written, nullptr) && written == sizeof(kMagic) &&
        WriteFile(hFile, &kVersion, sizeof(kVersion), &written, nullptr) && written == sizeof(kVersion) &&
        WriteFile(hFile, &count, sizeof(count), &written, nullptr) && written == sizeof(count);

    for (const auto& item : gHistory) {
        if (!saved) break;
        saved = WriteFile(hFile, &item.copiedAt, sizeof(SYSTEMTIME), &written, nullptr) && written == sizeof(SYSTEMTIME);
        uint32_t typeVal = static_cast<uint32_t>(item.type);
        saved = saved && WriteFile(hFile, &typeVal, sizeof(typeVal), &written, nullptr) && written == sizeof(typeVal);
        uint32_t pinnedVal = item.pinned ? 1 : 0;
        saved = saved && WriteFile(hFile, &pinnedVal, sizeof(pinnedVal), &written, nullptr) && written == sizeof(pinnedVal);

        saved = saved && WriteStorageString(hFile, item.preview) &&
            WriteStorageString(hFile, item.rawContent) &&
            WriteStorageString(hFile, item.source) &&
            WriteStorageString(hFile, item.pageTitle) &&
            WriteStorageString(hFile, item.pageUrl) &&
            WriteStorageString(hFile, item.imageFile);
    }

    if (saved) saved = FlushFileBuffers(hFile) != 0;
    CloseHandle(hFile);

    if (saved) saved = MoveFileExW(tmpPath.c_str(), datPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!saved) DeleteFileW(tmpPath.c_str());
}

void LoadHistoryFromDisk() {
    std::wstring appData = GetAppDataDirectory();
    if (appData.empty()) return;
    std::wstring datPath = appData + L"\\history.dat";
    std::wstring imgDir = appData + L"\\images\\";

    HANDLE hFile = CreateFileW(datPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD read = 0;
    uint32_t magic = 0, version = 0, count = 0;
    if (!ReadFile(hFile, &magic, sizeof(magic), &read, nullptr) || magic != 0x52544C43) {
        CloseHandle(hFile);
        return;
    }
    if (!ReadFile(hFile, &version, sizeof(version), &read, nullptr) || version != 1) {
        CloseHandle(hFile);
        return;
    }
    if (!ReadFile(hFile, &count, sizeof(count), &read, nullptr)) {
        CloseHandle(hFile);
        return;
    }

    std::vector<ClipItem> loaded;
    loaded.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        ClipItem item;
        if (!ReadFile(hFile, &item.copiedAt, sizeof(SYSTEMTIME), &read, nullptr) || read != sizeof(SYSTEMTIME)) break;
        uint32_t typeVal = 0;
        if (!ReadFile(hFile, &typeVal, sizeof(typeVal), &read, nullptr) || read != sizeof(typeVal)) break;
        item.type = static_cast<ClipItem::Type>(typeVal);
        uint32_t pinnedVal = 0;
        if (!ReadFile(hFile, &pinnedVal, sizeof(pinnedVal), &read, nullptr) || read != sizeof(pinnedVal)) break;
        item.pinned = (pinnedVal != 0);

        if (!ReadStorageString(hFile, item.preview)) break;
        if (!ReadStorageString(hFile, item.rawContent)) break;
        if (!ReadStorageString(hFile, item.source)) break;
        if (!ReadStorageString(hFile, item.pageTitle)) break;
        if (!ReadStorageString(hFile, item.pageUrl)) break;
        if (!ReadStorageString(hFile, item.imageFile)) break;

        if (item.type == ClipItem::Type::Image && !item.imageFile.empty()) {
            std::wstring fullImgPath = imgDir + item.imageFile;
            HBITMAP hBmp = static_cast<HBITMAP>(LoadImageW(nullptr, fullImgPath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
            if (hBmp) {
                item.image = std::shared_ptr<ClipItem::BitmapObject>(hBmp, [](ClipItem::BitmapObject* b) { DeleteObject(b); });
            }
        }

        loaded.push_back(std::move(item));
    }

    CloseHandle(hFile);

    if (!loaded.empty()) {
        std::stable_partition(loaded.begin(), loaded.end(), [](const ClipItem& it) { return it.pinned; });
        gHistory = std::move(loaded);
        gSelectedIndex = 0;
    }
}

void LoadSettings() {
    std::wstring appData = GetAppDataDirectory();
    if (appData.empty()) return;
    std::wstring iniPath = appData + L"\\settings.ini";

    int startWin = GetPrivateProfileIntW(L"General", L"StartWithWindows", 1, iniPath.c_str());
    gStartWithWindows = (startWin != 0);
    gReplaceWinV = GetPrivateProfileIntW(L"General", L"ReplaceWinV", 0, iniPath.c_str()) != 0;

    int retention = GetPrivateProfileIntW(L"General", L"Retention", 0, iniPath.c_str());
    if (retention >= 0 && retention <= 4) {
        gRetentionSetting = static_cast<HistoryRetention>(retention);
    } else {
        gRetentionSetting = HistoryRetention::Never;
    }
}

void SaveSettings() {
    std::wstring appData = GetAppDataDirectory();
    if (appData.empty()) return;
    std::wstring iniPath = appData + L"\\settings.ini";

    wchar_t buf[16]{};
    swprintf_s(buf, L"%d", gStartWithWindows ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"StartWithWindows", buf, iniPath.c_str());

    swprintf_s(buf, L"%d", static_cast<int>(gRetentionSetting));
    WritePrivateProfileStringW(L"General", L"Retention", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", gReplaceWinV ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"ReplaceWinV", buf, iniPath.c_str());
}

LRESULT CALLBACK WinVKeyboardProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
        if (key->vkCode == VK_LWIN || key->vkCode == VK_RWIN) {
            if (down) gWinKeyDown = true;
            if (up) gWinKeyDown = false;
        } else if (key->vkCode == VK_LCONTROL || key->vkCode == VK_RCONTROL || key->vkCode == VK_CONTROL) {
            if (down) gCtrlDown = true;
            if (up) gCtrlDown = false;
        } else if (key->vkCode == VK_LMENU || key->vkCode == VK_RMENU || key->vkCode == VK_MENU) {
            if (down) gAltDown = true;
            if (up) gAltDown = false;
        } else if (key->vkCode == 'V') {
            if (down && gWinVDown) return 1;
            if (down && gWinKeyDown && !gCtrlDown && !gAltDown) {
                if (!gWinVDown && gMainWindow) PostMessageW(gMainWindow, WM_OPEN_CLIPTRACE, 0, 0);
                gWinVDown = true;
                return 1;
            }
            if (up && gWinVDown) {
                gWinVDown = false;
                return 1;
            }
        }
    }
    return CallNextHookEx(gWinVHook, code, message, data);
}

bool SetWinVReplacement(bool enable) {
    if (enable) {
        if (!gWinVHook) gWinVHook = SetWindowsHookExW(WH_KEYBOARD_LL, WinVKeyboardProc, GetModuleHandleW(nullptr), 0);
        return gWinVHook != nullptr;
    }
    if (gWinVHook) UnhookWindowsHookEx(gWinVHook);
    gWinVHook = nullptr;
    gWinKeyDown = false;
    gWinVDown = false;
    gCtrlDown = false;
    gAltDown = false;
    return true;
}

bool IsAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH]{};
        DWORD size = sizeof(val);
        DWORD type = REG_SZ;
        LSTATUS status = RegQueryValueExW(hKey, L"ClipTrace", nullptr, &type, reinterpret_cast<LPBYTE>(val), &size);
        RegCloseKey(hKey);
        return (status == ERROR_SUCCESS && size > 0);
    }
    return false;
}

void SetAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH]{};
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
                std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
                RegSetValueExW(hKey, L"ClipTrace", 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted.c_str()), static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
            }
        } else {
            RegDeleteValueW(hKey, L"ClipTrace");
        }
        RegCloseKey(hKey);
    }
}

ULONGLONG RetentionToSeconds(HistoryRetention r) {
    switch (r) {
    case HistoryRetention::TwoHours: return 2ULL * 3600ULL;
    case HistoryRetention::OneDay: return 24ULL * 3600ULL;
    case HistoryRetention::OneWeek: return 7ULL * 24ULL * 3600ULL;
    case HistoryRetention::OneMonth: return 30ULL * 24ULL * 3600ULL;
    default: return 0ULL;
    }
}

bool IsItemExpired(const SYSTEMTIME& itemTime, HistoryRetention retention) {
    if (retention == HistoryRetention::Never) return false;
    FILETIME ftItem{};
    if (!SystemTimeToFileTime(&itemTime, &ftItem)) return false;
    ULARGE_INTEGER uItem;
    uItem.LowPart = ftItem.dwLowDateTime;
    uItem.HighPart = ftItem.dwHighDateTime;

    SYSTEMTIME nowSys{};
    GetLocalTime(&nowSys);
    FILETIME ftNow{};
    if (!SystemTimeToFileTime(&nowSys, &ftNow)) return false;
    ULARGE_INTEGER uNow;
    uNow.LowPart = ftNow.dwLowDateTime;
    uNow.HighPart = ftNow.dwHighDateTime;

    if (uNow.QuadPart <= uItem.QuadPart) return false;
    ULONGLONG diff100ns = uNow.QuadPart - uItem.QuadPart;
    ULONGLONG diffSec = diff100ns / 10000000ULL;
    return diffSec >= RetentionToSeconds(retention);
}

bool ApplyAutoCleanup() {
    if (gRetentionSetting == HistoryRetention::Never) return false;
    bool changed = false;
    std::wstring appData = GetAppDataDirectory();
    std::wstring imgDir = appData.empty() ? L"" : (appData + L"\\images\\");

    for (auto it = gHistory.begin(); it != gHistory.end(); ) {
        if (!it->pinned && IsItemExpired(it->copiedAt, gRetentionSetting)) {
            if (!it->imageFile.empty() && !imgDir.empty()) {
                DeleteFileW((imgDir + it->imageFile).c_str());
            }
            it = gHistory.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        if (gSelectedIndex >= static_cast<int>(gHistory.size())) {
            gSelectedIndex = gHistory.empty() ? -1 : 0;
        }
        SaveHistoryToDisk();
    }
    return changed;
}

void ClearAllHistory() {
    std::wstring appData = GetAppDataDirectory();
    if (!appData.empty()) {
        std::wstring imgDir = appData + L"\\images\\";
        for (const auto& item : gHistory) {
            if (!item.imageFile.empty()) {
                DeleteFileW((imgDir + item.imageFile).c_str());
            }
        }
    }
    gHistory.clear();
    gSelectedIndex = -1;
    SaveHistoryToDisk();
}

void CloseDetailModal(HWND hwnd, DetailDialogState* state) {
    if (!state || state->isClosing) return;
    state->isClosing = true;
    state->closeProgress = 0.0f;
    SetTimer(hwnd, kDetailAnimTimer, kDetailAnimInterval, nullptr);
}

LRESULT CALLBACK DetailWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DetailDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<DetailDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        if (wParam != kDetailAnimTimer) break;
        bool animating = false;

        // Window open animation: smooth slide up with cubic deceleration
        if (state->openProgress < 1.0f) {
            state->openProgress += (1.0f - state->openProgress) * 0.25f + 0.02f;
            if (state->openProgress >= 1.0f) state->openProgress = 1.0f;
            const float t = state->openProgress;
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            const int curY = state->targetY + static_cast<int>(18.0f * (1.0f - eased));
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        // Window close animation: smooth slide down
        if (state->isClosing) {
            state->closeProgress += (1.0f - state->closeProgress) * 0.28f + 0.04f;
            if (state->closeProgress >= 1.0f) {
                KillTimer(hwnd, kDetailAnimTimer);
                DestroyWindow(hwnd);
                return 0;
            }
            const float t = state->closeProgress;
            const float eased = t * t;
            const int curY = state->targetY + static_cast<int>(18.0f * eased);
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        // Smooth hover animations
        constexpr float kHoverSpeed = 0.22f;
        auto stepVal = [&](float& cur, float target) {
            if (std::abs(cur - target) > 0.005f) {
                cur += (target - cur) * kHoverSpeed;
                animating = true;
            } else {
                cur = target;
            }
        };

        stepVal(state->closeHoverProgress, state->closeHovered ? 1.0f : 0.0f);
        stepVal(state->contentHoverProgress, state->contentHovered ? 1.0f : 0.0f);
        stepVal(state->urlHoverProgress, state->urlHovered ? 1.0f : 0.0f);

        InvalidateRect(hwnd, nullptr, FALSE);

        if (!animating && !state->isClosing && state->openProgress >= 1.0f) {
            KillTimer(hwnd, kDetailAnimTimer);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (IsInside(point, state->closeRect)) return HTCLIENT;
        return point.y < 44 ? HTCAPTION : HTCLIENT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC realDc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC dc = CreateCompatibleDC(realDc);
        HBITMAP memBitmap = CreateCompatibleBitmap(realDc, width, height);
        HGDIOBJ oldMemBitmap = SelectObject(dc, memBitmap);

        HBRUSH background = CreateSolidBrush(kWindowBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);

        state->closeRect = {client.right - 44, 0, client.right, 44};
        DrawTextLine(dc, L"Detalhes do item", {18, 0, 280, 44}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (state->closeHoverProgress > 0.01f) {
            COLORREF closeBg = LerpColor(kWindowBackground, RGB(49, 62, 91), state->closeHoverProgress);
            FillRoundedRect(dc, {state->closeRect.left + 5, state->closeRect.top + 6, state->closeRect.right - 5, state->closeRect.bottom - 6}, 4, closeBg);
        }
        COLORREF closeTextColor = LerpColor(kText, RGB(255, 255, 255), state->closeHoverProgress);
        DrawTextLine(dc, ClipTraceIcons::kClose, state->closeRect, gIconFont, closeTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLine(dc, 0, 43, client.right, 43, RGB(60, 73, 101));

        wchar_t timestamp[32]{};
        swprintf_s(timestamp, L"%02u/%02u/%04u  %02u:%02u:%02u", state->item.copiedAt.wDay, state->item.copiedAt.wMonth, state->item.copiedAt.wYear, state->item.copiedAt.wHour, state->item.copiedAt.wMinute, state->item.copiedAt.wSecond);
        const std::wstring typeAndTime = std::wstring(TypeName(state->item.type)) + L"  |  " + timestamp;
        COLORREF parsedColor{};
        const bool isColor = !state->item.image && TryParseColor(state->item.rawContent, parsedColor);

        const wchar_t* headerTitle = state->item.image ? L"CONTEÚDO DA IMAGEM" : (isColor ? L"PREVIEW DA COR COPIADA" : L"CONTEÚDO");
        DrawTextLine(dc, headerTitle, {20, 62, client.right - 20, 82}, gHeaderFont, isColor ? kAccent : kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        
        RECT cardRect = {20, 88, client.right - 20, 166};
        state->contentRect = cardRect;
        COLORREF baseCardBg = kCardBackground;
        COLORREF hoverCardBg = (state->item.image || isColor) ? RGB(38, 52, 78) : RGB(34, 44, 64);
        COLORREF cardBg = LerpColor(baseCardBg, hoverCardBg, state->contentHoverProgress);
        FillRoundedRect(dc, cardRect, 8, cardBg);

        if (state->contentHoverProgress > 0.01f) {
            COLORREF borderCol = LerpColor(baseCardBg, kAccent, state->contentHoverProgress);
            HPEN borderPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldPen = SelectObject(dc, borderPen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 8, 8);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(borderPen);
        }

        if (state->item.image) {
            BITMAP bitmap{};
            GetObjectW(state->item.image.get(), sizeof(bitmap), &bitmap);
            HDC memoryDc = CreateCompatibleDC(dc);
            HGDIOBJ oldBitmap = SelectObject(memoryDc, state->item.image.get());
            SetStretchBltMode(dc, HALFTONE);

            const int thumbX = cardRect.left + 12;
            const int thumbY = cardRect.top + 9;
            const int thumbSize = 60;
            StretchBlt(dc, thumbX, thumbY, thumbSize, thumbSize, memoryDc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteDC(memoryDc);

            HPEN thumbPen = CreatePen(PS_SOLID, 1, LerpColor(RGB(45, 56, 78), kAccent, state->contentHoverProgress));
            HGDIOBJ oldP = SelectObject(dc, thumbPen);
            HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, thumbX - 1, thumbY - 1, thumbX + thumbSize + 1, thumbY + thumbSize + 1);
            SelectObject(dc, oldB);
            SelectObject(dc, oldP);
            DeleteObject(thumbPen);

            DrawTextLine(dc, state->item.preview.c_str(), {cardRect.left + 84, cardRect.top + 12, cardRect.right - 14, cardRect.top + 36}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            wchar_t dimText[64]{};
            swprintf_s(dimText, L"%d \x00D7 %d px", bitmap.bmWidth, bitmap.bmHeight);
            COLORREF subTextColor = LerpColor(kMuted, RGB(180, 200, 230), state->contentHoverProgress);

            RECT dimCalc{};
            HGDIOBJ oldF = SelectObject(dc, gSmallFont);
            DrawTextW(dc, dimText, -1, &dimCalc, DT_CALCRECT | DT_SINGLELINE);
            SelectObject(dc, oldF);
            int dimW = dimCalc.right - dimCalc.left;

            DrawTextLine(dc, dimText, {cardRect.left + 84, cardRect.top + 40, cardRect.left + 84 + dimW, cardRect.top + 68}, gSmallFont, subTextColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            int badgeLeft = cardRect.left + 84 + dimW + 16;
            int badgeWidth = 195;
            RECT badgeRect = {badgeLeft, cardRect.top + 42, badgeLeft + badgeWidth, cardRect.top + 66};

            COLORREF badgeBg = LerpColor(RGB(32, 42, 60), RGB(0, 102, 204), state->contentHoverProgress);
            COLORREF badgeBorder = LerpColor(RGB(50, 65, 90), RGB(50, 140, 230), state->contentHoverProgress);
            COLORREF badgeText = LerpColor(RGB(150, 165, 185), RGB(255, 255, 255), state->contentHoverProgress);

            FillRoundedRect(dc, badgeRect, 4, badgeBg);
            HPEN bPen = CreatePen(PS_SOLID, 1, badgeBorder);
            HGDIOBJ oP = SelectObject(dc, bPen);
            HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 4, 4);
            SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(bPen);

            DrawTextLine(dc, L"\xE8A7  Abrir no visualizador", badgeRect, gSmallFont, badgeText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (isColor) {
            const int swatchX = cardRect.left + 12;
            const int swatchY = cardRect.top + 9;
            const int swatchSize = 60;
            RECT swatchRect = {swatchX, swatchY, swatchX + swatchSize, swatchY + swatchSize};
            FillRoundedRect(dc, swatchRect, 8, parsedColor);

            int brightness = (GetRValue(parsedColor) * 299 + GetGValue(parsedColor) * 587 + GetBValue(parsedColor) * 114) / 1000;
            COLORREF borderCol = brightness > 140 ? RGB(50, 65, 90) : RGB(140, 170, 220);
            HPEN sPen = CreatePen(PS_SOLID, 1, LerpColor(borderCol, kAccent, state->contentHoverProgress));
            HGDIOBJ oP = SelectObject(dc, sPen);
            HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, swatchRect.left, swatchRect.top, swatchRect.right, swatchRect.bottom, 8, 8);
            SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(sPen);

            wchar_t hexStr[32]{};
            swprintf_s(hexStr, L"#%02X%02X%02X", GetRValue(parsedColor), GetGValue(parsedColor), GetBValue(parsedColor));
            DrawTextLine(dc, hexStr, {cardRect.left + 84, cardRect.top + 10, cardRect.right - 14, cardRect.top + 34}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            wchar_t rgbStr[48]{};
            swprintf_s(rgbStr, L"RGB: %d, %d, %d", GetRValue(parsedColor), GetGValue(parsedColor), GetBValue(parsedColor));
            float h = 0, s = 0, l = 0;
            RgbToHsl(parsedColor, h, s, l);
            wchar_t hslStr[48]{};
            swprintf_s(hslStr, L"HSL: %.0f\x00B0, %.0f%%, %.0f%%", h, s * 100.0f, l * 100.0f);

            std::wstring compStr = std::wstring(rgbStr) + L"   |   " + hslStr;
            COLORREF subCol = LerpColor(kMuted, RGB(180, 205, 235), state->contentHoverProgress);
            DrawTextLine(dc, compStr.c_str(), {cardRect.left + 84, cardRect.top + 34, cardRect.right - 14, cardRect.top + 52}, gSmallFont, subCol, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            int badgeLeft = cardRect.left + 84;
            int badgeWidth = 160;
            RECT copyBadge = {badgeLeft, cardRect.top + 54, badgeLeft + badgeWidth, cardRect.top + 73};
            COLORREF badgeBg = LerpColor(RGB(32, 42, 60), RGB(0, 102, 204), state->contentHoverProgress);
            COLORREF badgeBorder = LerpColor(RGB(50, 65, 90), RGB(50, 140, 230), state->contentHoverProgress);
            COLORREF badgeText = LerpColor(RGB(150, 165, 185), RGB(255, 255, 255), state->contentHoverProgress);

            FillRoundedRect(dc, copyBadge, 4, badgeBg);
            HPEN bPen = CreatePen(PS_SOLID, 1, badgeBorder);
            HGDIOBJ obP = SelectObject(dc, bPen);
            HGDIOBJ obB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, copyBadge.left, copyBadge.top, copyBadge.right, copyBadge.bottom, 4, 4);
            SelectObject(dc, obB); SelectObject(dc, obP); DeleteObject(bPen);

            DrawTextLine(dc, L"\xE8C8  Copiar valor HEX", copyBadge, gSmallFont, badgeText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            COLORREF textCol = LerpColor(kText, RGB(255, 255, 255), state->contentHoverProgress);
            DrawTextLine(dc, state->item.rawContent.c_str(), {32, 98, client.right - 32, 156}, gBodyFont, textCol, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        }

        DrawTextLine(dc, typeAndTime.c_str(), {20, 182, client.right - 20, 204}, gSmallFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"ORIGEM", {20, 222, client.right - 20, 241}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, state->item.source.c_str(), {20, 242, client.right - 20, 265}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextLine(dc, L"TÍTULO DA PÁGINA OU JANELA", {20, 279, client.right - 20, 298}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, state->item.pageTitle.empty() ? L"Não disponível" : state->item.pageTitle.c_str(), {20, 299, client.right - 20, 322}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const bool hasUrl = !state->item.pageUrl.empty() && state->item.pageUrl != L"Não disponível";
        const bool isWeb = hasUrl && (state->item.pageUrl.rfind(L"http://", 0) == 0 || state->item.pageUrl.rfind(L"https://", 0) == 0);
        const wchar_t* urlHeader = isWeb ? L"URL (CLIQUE PARA ABRIR NO NAVEGADOR)" : (hasUrl ? L"DIRETÓRIO DO APP (CLIQUE PARA ABRIR)" : L"URL");
        DrawTextLine(dc, urlHeader, {20, 336, client.right - 20, 355}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        state->urlRect = {20, 356, client.right - 20, 381};
        COLORREF urlColor = hasUrl ? LerpColor(kAccent, RGB(140, 215, 255), state->urlHoverProgress) : kMuted;
        DrawTextLine(dc, hasUrl ? state->item.pageUrl.c_str() : L"Não disponível", state->urlRect, gSmallFont, urlColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (hasUrl && state->urlHoverProgress > 0.01f) {
            RECT calc = state->urlRect;
            HGDIOBJ oldF = SelectObject(dc, gSmallFont);
            DrawTextW(dc, state->item.pageUrl.c_str(), -1, &calc, DT_CALCRECT | DT_LEFT | DT_SINGLELINE);
            SelectObject(dc, oldF);
            int maxW = min(calc.right - calc.left, state->urlRect.right - state->urlRect.left);
            int animW = static_cast<int>(maxW * state->urlHoverProgress);
            COLORREF lineCol = LerpColor(kAccent, RGB(140, 215, 255), state->urlHoverProgress);
            DrawLine(dc, state->urlRect.left, state->urlRect.bottom - 2, state->urlRect.left + animW, state->urlRect.bottom - 2, lineCol, 1);
        }

        BitBlt(realDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldMemBitmap);
        DeleteObject(memBitmap);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool hasUrl = !state->item.pageUrl.empty() && state->item.pageUrl != L"Não disponível";
        const bool uHover = hasUrl && IsInside(point, state->urlRect);
        const bool cHover = IsInside(point, state->closeRect);
        COLORREF dummyCol{};
        const bool isCol = !state->item.image && TryParseColor(state->item.rawContent, dummyCol);
        const bool contentHov = (state->item.image || isCol) && IsInside(point, state->contentRect);
        if (uHover != state->urlHovered || cHover != state->closeHovered || contentHov != state->contentHovered) {
            state->urlHovered = uHover;
            state->closeHovered = cHover;
            state->contentHovered = contentHov;
            SetTimer(hwnd, kDetailAnimTimer, kDetailAnimInterval, nullptr);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (state->urlHovered || state->closeHovered || state->contentHovered) {
            state->urlHovered = false;
            state->closeHovered = false;
            state->contentHovered = false;
            SetTimer(hwnd, kDetailAnimTimer, kDetailAnimInterval, nullptr);
        }
        return 0;
    }
    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        const bool hasUrl = !state->item.pageUrl.empty() && state->item.pageUrl != L"Não disponível";
        if (hasUrl && IsInside(point, state->urlRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        COLORREF dummyCol{};
        const bool isCol = !state->item.image && TryParseColor(state->item.rawContent, dummyCol);
        if ((state->item.image || isCol) && IsInside(point, state->contentRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsInside(point, state->closeRect)) {
            CloseDetailModal(hwnd, state);
            return 0;
        }
        if (state->item.image && IsInside(point, state->contentRect)) {
            OpenImageExternal(state->item);
            return 0;
        }
        COLORREF clickedColor{};
        if (!state->item.image && TryParseColor(state->item.rawContent, clickedColor) && IsInside(point, state->contentRect)) {
            wchar_t hexBuf[32]{};
            swprintf_s(hexBuf, L"#%02X%02X%02X", GetRValue(clickedColor), GetGValue(clickedColor), GetBValue(clickedColor));
            ClipItem copyItem = state->item;
            copyItem.rawContent = hexBuf;
            copyItem.preview = hexBuf;
            CopyItemToClipboard(hwnd, copyItem);
            ShowSoftToast(L"Valor HEX copiado com sucesso!");
            return 0;
        }
        const bool hasUrl = !state->item.pageUrl.empty() && state->item.pageUrl != L"Não disponível";
        if (hasUrl && IsInside(point, state->urlRect)) {
            const bool isWeb = state->item.pageUrl.rfind(L"http://", 0) == 0 || state->item.pageUrl.rfind(L"https://", 0) == 0;
            ShowSoftToast(isWeb ? L"Abrindo link no navegador..." : L"Abrindo diretório no Explorer...");
            ShellExecuteW(nullptr, L"open", state->item.pageUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) CloseDetailModal(hwnd, state);
        return 0;
    case WM_CLOSE:
        CloseDetailModal(hwnd, state);
        return 0;
    case WM_NCDESTROY:
        gActiveModalHwnd = nullptr;
        EnableWindow(state->owner, TRUE);
        ShowWindow(state->owner, SW_SHOW);
        SetForegroundWindow(state->owner);
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowDetails(HWND hwnd, const ClipItem& item) {
    auto* state = new DetailDialogState{item, hwnd};
    RECT ownerRect{};
    GetWindowRect(hwnd, &ownerRect);
    constexpr int dialogWidth = 500;
    constexpr int dialogHeight = 410;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - dialogWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - dialogHeight) / 2;
    state->targetX = x;
    state->targetY = y;
    state->openProgress = 0.0f;
    state->closeProgress = 0.0f;
    state->isClosing = false;

    HWND dialog = CreateWindowExW(WS_EX_TOOLWINDOW, L"ClipTraceDetailWindow", L"Detalhes do item", WS_POPUP,
        x, y + 18, dialogWidth, dialogHeight, hwnd, nullptr, GetModuleHandleW(nullptr), state);
    if (!dialog) { delete state; return; }
    gActiveModalHwnd = dialog;
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(dialog, 20, &darkMode, sizeof(darkMode));
    const int roundedCorners = 2;
    DwmSetWindowAttribute(dialog, 33, &roundedCorners, sizeof(roundedCorners));
    const int micaBackdrop = 2;
    DwmSetWindowAttribute(dialog, 38, &micaBackdrop, sizeof(micaBackdrop));
    EnableWindow(hwnd, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetTimer(dialog, kDetailAnimTimer, kDetailAnimInterval, nullptr);
}

void CloseSettingsModal(HWND hwnd, SettingsDialogState* state) {
    if (!state || state->isClosing) return;
    state->isClosing = true;
    state->closeProgress = 0.0f;
    SetTimer(hwnd, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<SettingsDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        if (wParam != kSettingsAnimTimer) break;
        bool animating = false;

        // Window open animation: smooth slide up with cubic deceleration
        if (state->openProgress < 1.0f) {
            state->openProgress += (1.0f - state->openProgress) * 0.25f + 0.02f;
            if (state->openProgress >= 1.0f) state->openProgress = 1.0f;
            const float t = state->openProgress;
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            const int curY = state->targetY + static_cast<int>(18.0f * (1.0f - eased));
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        // Window close animation: smooth slide down
        if (state->isClosing) {
            state->closeProgress += (1.0f - state->closeProgress) * 0.28f + 0.04f;
            if (state->closeProgress >= 1.0f) {
                KillTimer(hwnd, kSettingsAnimTimer);
                DestroyWindow(hwnd);
                return 0;
            }
            const float t = state->closeProgress;
            const float eased = t * t;
            const int curY = state->targetY + static_cast<int>(18.0f * eased);
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        // Smooth hover animations
        constexpr float kHoverSpeed = 0.22f;
        auto stepVal = [&](float& cur, float target) {
            if (std::abs(cur - target) > 0.005f) {
                cur += (target - cur) * kHoverSpeed;
                animating = true;
            } else {
                cur = target;
            }
        };

        stepVal(state->closeHoverProgress, state->closeHovered ? 1.0f : 0.0f);
        stepVal(state->clearHoverProgress, state->clearHovered ? 1.0f : 0.0f);
        stepVal(state->switchHoverProgress, state->switchHovered ? 1.0f : 0.0f);
        stepVal(state->winVHoverProgress, state->winVHovered ? 1.0f : 0.0f);

        for (int i = 0; i < 5; ++i) {
            stepVal(state->tabHoverProgress[i], state->tabHovered == i ? 1.0f : 0.0f);
        }

        const float targetSwitch = gStartWithWindows ? 1.0f : 0.0f;
        if (std::abs(state->switchToggleProgress - targetSwitch) > 0.005f) {
            state->switchToggleProgress += (targetSwitch - state->switchToggleProgress) * 0.25f;
            animating = true;
        } else {
            state->switchToggleProgress = targetSwitch;
        }
        const float targetWinV = gReplaceWinV ? 1.0f : 0.0f;
        if (std::abs(state->winVToggleProgress - targetWinV) > 0.005f) {
            state->winVToggleProgress += (targetWinV - state->winVToggleProgress) * 0.25f;
            animating = true;
        } else {
            state->winVToggleProgress = targetWinV;
        }

        InvalidateRect(hwnd, nullptr, FALSE);

        if (!animating && !state->isClosing && state->openProgress >= 1.0f) {
            KillTimer(hwnd, kSettingsAnimTimer);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (IsInside(point, state->closeRect)) return HTCLIENT;
        return point.y < 44 ? HTCAPTION : HTCLIENT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC realDc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC dc = CreateCompatibleDC(realDc);
        HBITMAP memBitmap = CreateCompatibleBitmap(realDc, width, height);
        HGDIOBJ oldMemBitmap = SelectObject(dc, memBitmap);

        HBRUSH background = CreateSolidBrush(kWindowBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);

        // --- Header (0 to 44px) ---
        state->closeRect = {client.right - 44, 0, client.right, 44};
        DrawTextLine(dc, L"\xE713", {18, 0, 42, 44}, gIconFont, kAccent, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"Configurações", {46, 0, 320, 44}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (state->closeHoverProgress > 0.01f) {
            COLORREF closeBg = LerpColor(kWindowBackground, RGB(49, 62, 91), state->closeHoverProgress);
            FillRoundedRect(dc, {state->closeRect.left + 5, state->closeRect.top + 6, state->closeRect.right - 5, state->closeRect.bottom - 6}, 4, closeBg);
        }
        COLORREF closeTextColor = LerpColor(kText, RGB(255, 255, 255), state->closeHoverProgress);
        DrawTextLine(dc, ClipTraceIcons::kClose, state->closeRect, gIconFont, closeTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLine(dc, 0, 43, client.right, 43, RGB(60, 73, 101));

        // --- Section 1: HISTÓRICO E ARMAZENAMENTO ---
        DrawTextLine(dc, L"HISTÓRICO E ARMAZENAMENTO", {20, 56, client.right - 20, 74}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT histCard = {20, 78, client.right - 20, 168};
        FillRoundedRect(dc, histCard, 8, kCardBackground);

        DrawTextLine(dc, L"Seus clipes são salvos de forma segura no %AppData%.", {histCard.left + 16, histCard.top + 10, histCard.right - 16, histCard.top + 30}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        wchar_t countStr[64]{};
        swprintf_s(countStr, L"Itens armazenados atualmente: %zu item(ns)", gHistory.size());
        DrawTextLine(dc, countStr, {histCard.left + 16, histCard.top + 32, histCard.right - 16, histCard.top + 50}, gSmallFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        state->clearHistoryRect = {histCard.left + 16, histCard.bottom - 36, histCard.left + 230, histCard.bottom - 8};
        COLORREF clearBtnBg = LerpColor(RGB(48, 58, 80), RGB(168, 42, 52), state->clearHoverProgress);
        FillRoundedRect(dc, state->clearHistoryRect, 6, clearBtnBg);

        COLORREF clearBorder = LerpColor(RGB(65, 80, 108), RGB(220, 70, 80), state->clearHoverProgress);
        HPEN cPen = CreatePen(PS_SOLID, 1, clearBorder);
        HGDIOBJ oldP = SelectObject(dc, cPen);
        HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, state->clearHistoryRect.left, state->clearHistoryRect.top, state->clearHistoryRect.right, state->clearHistoryRect.bottom, 6, 6);
        SelectObject(dc, oldB); SelectObject(dc, oldP); DeleteObject(cPen);

        COLORREF clearTxtColor = LerpColor(kText, RGB(255, 255, 255), state->clearHoverProgress);
        DrawTextLine(dc, ClipTraceIcons::kDelete, {state->clearHistoryRect.left + 8, state->clearHistoryRect.top, state->clearHistoryRect.left + 28, state->clearHistoryRect.bottom}, gIconFont, clearTxtColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"Apagar todo o histórico", {state->clearHistoryRect.left + 30, state->clearHistoryRect.top, state->clearHistoryRect.right - 8, state->clearHistoryRect.bottom}, gSmallFont, clearTxtColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // --- Section 2: LIMPEZA AUTOMÁTICA ---
        DrawTextLine(dc, L"LIMPEZA AUTOMÁTICA (RETENÇÃO)", {20, 180, client.right - 20, 198}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT retCard = {20, 202, client.right - 20, 310};
        FillRoundedRect(dc, retCard, 8, kCardBackground);

        DrawTextLine(dc, L"Define por quanto tempo os itens não fixados são mantidos:", {retCard.left + 16, retCard.top + 10, retCard.right - 16, retCard.top + 28}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const wchar_t* tabLabels[5] = { L"Não apagar", L"2 Horas", L"1 Dia", L"1 Semana", L"1 Mês" };
        const int tabCardInnerW = (retCard.right - 16) - (retCard.left + 16);
        const int tabGap = 6;
        const int tabWidth = (tabCardInnerW - (tabGap * 4)) / 5;
        const int tabY = retCard.top + 34;

        for (int i = 0; i < 5; ++i) {
            RECT tabR = { retCard.left + 16 + i * (tabWidth + tabGap), tabY, retCard.left + 16 + i * (tabWidth + tabGap) + tabWidth, tabY + 32 };
            state->retentionTabs[i] = tabR;
            const bool isSelected = static_cast<int>(gRetentionSetting) == i;

            COLORREF tabBg;
            COLORREF tabBorder;
            COLORREF tabText;

            if (isSelected) {
                tabBg = RGB(36, 92, 168);
                tabBorder = kAccent;
                tabText = RGB(255, 255, 255);
            } else {
                tabBg = LerpColor(RGB(28, 38, 58), RGB(48, 64, 96), state->tabHoverProgress[i]);
                tabBorder = LerpColor(RGB(50, 65, 90), RGB(80, 105, 145), state->tabHoverProgress[i]);
                tabText = LerpColor(kMuted, RGB(245, 247, 252), state->tabHoverProgress[i]);
            }

            FillRoundedRect(dc, tabR, 6, tabBg);

            HPEN tPen = CreatePen(PS_SOLID, 1, tabBorder);
            HGDIOBJ oP = SelectObject(dc, tPen);
            HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, tabR.left, tabR.top, tabR.right, tabR.bottom, 6, 6);
            SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(tPen);

            DrawTextLine(dc, tabLabels[i], tabR, gSmallFont, tabText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        const wchar_t* retentionDescriptions[5] = {
            L"Itens copiados nunca serão apagados automaticamente.",
            L"Itens não fixados copiados há mais de 2 horas serão apagados.",
            L"Itens não fixados copiados há mais de 1 dia serão apagados.",
            L"Itens não fixados copiados há mais de 1 semana serão apagados.",
            L"Itens não fixados copiados há mais de 1 mês serão apagados."
        };
        const wchar_t* curDesc = retentionDescriptions[min(max(0, static_cast<int>(gRetentionSetting)), 4)];
        DrawTextLine(dc, curDesc, {retCard.left + 16, retCard.bottom - 30, retCard.right - 16, retCard.bottom - 10}, gSmallFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // --- Section 3: SISTEMA E SEGUNDO PLANO ---
        DrawTextLine(dc, L"SEGUNDO PLANO E SISTEMA", {20, 322, client.right - 20, 340}, gHeaderFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT autoCard = {20, 344, client.right - 20, 426};
        state->autoStartRowRect = autoCard;
        FillRoundedRect(dc, autoCard, 8, kCardBackground);

        DrawTextLine(dc, L"Iniciar com o Windows em segundo plano", {autoCard.left + 16, autoCard.top + 16, autoCard.right - 80, autoCard.top + 38}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"Permite monitorar a área de transferência e rodar na bandeja do sistema.", {autoCard.left + 16, autoCard.top + 38, autoCard.right - 80, autoCard.top + 58}, gSmallFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        state->autoStartSwitchRect = {autoCard.right - 66, autoCard.top + 26, autoCard.right - 18, autoCard.top + 54};
        COLORREF switchBg = LerpColor(RGB(48, 58, 78), RGB(0, 120, 215), state->switchToggleProgress);
        if (state->switchHoverProgress > 0.01f) {
            switchBg = LerpColor(switchBg, RGB(35, 145, 240), state->switchHoverProgress * state->switchToggleProgress);
        }
        FillRoundedRect(dc, state->autoStartSwitchRect, 14, switchBg);

        COLORREF switchBorder = LerpColor(RGB(75, 90, 115), RGB(99, 190, 255), state->switchToggleProgress);
        HPEN swPen = CreatePen(PS_SOLID, 1, switchBorder);
        HGDIOBJ oSp = SelectObject(dc, swPen);
        HGDIOBJ oSb = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, state->autoStartSwitchRect.left, state->autoStartSwitchRect.top, state->autoStartSwitchRect.right, state->autoStartSwitchRect.bottom, 14, 14);
        SelectObject(dc, oSb); SelectObject(dc, oSp); DeleteObject(swPen);

        const int travelW = (state->autoStartSwitchRect.right - state->autoStartSwitchRect.left) - 28;
        const int thumbX = state->autoStartSwitchRect.left + 4 + static_cast<int>(travelW * state->switchToggleProgress);
        const int thumbY = state->autoStartSwitchRect.top + 4;
        RECT thumbRect = {thumbX, thumbY, thumbX + 20, thumbY + 20};
        FillRoundedRect(dc, thumbRect, 10, RGB(255, 255, 255));

        RECT winVCard = {20, 438, client.right - 20, 520};
        state->winVRowRect = winVCard;
        FillRoundedRect(dc, winVCard, 8, kCardBackground);
        DrawTextLine(dc, L"Usar ClipTrace com Win + V", {winVCard.left + 16, winVCard.top + 16, winVCard.right - 80, winVCard.top + 38}, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"Abre o ClipTrace no lugar do histórico do Windows.", {winVCard.left + 16, winVCard.top + 38, winVCard.right - 80, winVCard.top + 58}, gSmallFont, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        state->winVSwitchRect = {winVCard.right - 66, winVCard.top + 26, winVCard.right - 18, winVCard.top + 54};
        const COLORREF winVSwitchBg = LerpColor(RGB(48, 58, 78), RGB(0, 120, 215), state->winVToggleProgress);
        FillRoundedRect(dc, state->winVSwitchRect, 14, LerpColor(winVSwitchBg, RGB(35, 145, 240), state->winVHoverProgress * state->winVToggleProgress));
        HPEN winVPen = CreatePen(PS_SOLID, 1, LerpColor(RGB(75, 90, 115), kAccent, state->winVToggleProgress));
        HGDIOBJ oldWinVPen = SelectObject(dc, winVPen);
        HGDIOBJ oldWinVBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, state->winVSwitchRect.left, state->winVSwitchRect.top, state->winVSwitchRect.right, state->winVSwitchRect.bottom, 14, 14);
        SelectObject(dc, oldWinVBrush);
        SelectObject(dc, oldWinVPen);
        DeleteObject(winVPen);
        const int winVThumbX = state->winVSwitchRect.left + 4 + static_cast<int>(20 * state->winVToggleProgress);
        FillRoundedRect(dc, {winVThumbX, state->winVSwitchRect.top + 4, winVThumbX + 20, state->winVSwitchRect.top + 24}, 10, RGB(255, 255, 255));

        BitBlt(realDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldMemBitmap);
        DeleteObject(memBitmap);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool cHov = IsInside(point, state->closeRect);
        const bool clrHov = IsInside(point, state->clearHistoryRect);
        int tHov = -1;
        for (int i = 0; i < 5; ++i) {
            if (IsInside(point, state->retentionTabs[i])) {
                tHov = i;
                break;
            }
        }
        const bool swHov = IsInside(point, state->autoStartSwitchRect) || IsInside(point, state->autoStartRowRect);
        const bool winVHov = IsInside(point, state->winVRowRect);

        if (cHov != state->closeHovered || clrHov != state->clearHovered || tHov != state->tabHovered || swHov != state->switchHovered || winVHov != state->winVHovered) {
            state->closeHovered = cHov;
            state->clearHovered = clrHov;
            state->tabHovered = tHov;
            state->switchHovered = swHov;
            state->winVHovered = winVHov;
            SetTimer(hwnd, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (state->closeHovered || state->clearHovered || state->tabHovered != -1 || state->switchHovered || state->winVHovered) {
            state->closeHovered = false;
            state->clearHovered = false;
            state->tabHovered = -1;
            state->switchHovered = false;
            state->winVHovered = false;
            SetTimer(hwnd, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        if (IsInside(point, state->closeRect) || IsInside(point, state->clearHistoryRect) || IsInside(point, state->autoStartRowRect) || IsInside(point, state->winVRowRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        for (int i = 0; i < 5; ++i) {
            if (IsInside(point, state->retentionTabs[i])) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsInside(point, state->closeRect)) {
            CloseSettingsModal(hwnd, state);
            return 0;
        }
        if (IsInside(point, state->clearHistoryRect)) {
            ClearAllHistory();
            ShowSoftToast(L"Histórico apagado com sucesso.");
            InvalidateRect(hwnd, nullptr, FALSE);
            InvalidateRect(state->owner, nullptr, FALSE);
            return 0;
        }
        for (int i = 0; i < 5; ++i) {
            if (IsInside(point, state->retentionTabs[i])) {
                gRetentionSetting = static_cast<HistoryRetention>(i);
                SaveSettings();
                ApplyAutoCleanup();
                InvalidateRect(hwnd, nullptr, FALSE);
                InvalidateRect(state->owner, nullptr, FALSE);
                const wchar_t* tabToast[5] = {
                    L"Retenção: Não apagar automaticamente",
                    L"Retenção: Apagar a cada 2 horas",
                    L"Retenção: Apagar a cada 1 dia",
                    L"Retenção: Apagar a cada 1 semana",
                    L"Retenção: Apagar a cada 1 mês"
                };
                ShowSoftToast(tabToast[i]);
                return 0;
            }
        }
        if (IsInside(point, state->autoStartSwitchRect) || IsInside(point, state->autoStartRowRect)) {
            gStartWithWindows = !gStartWithWindows;
            SetAutoStartEnabled(gStartWithWindows);
            SaveSettings();
            SetTimer(hwnd, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
            ShowSoftToast(gStartWithWindows ? L"Inicialização com o Windows ativada." : L"Inicialização com o Windows desativada.");
            return 0;
        }
        if (IsInside(point, state->winVRowRect)) {
            const bool enabled = !gReplaceWinV;
            if (SetWinVReplacement(enabled)) {
                gReplaceWinV = enabled;
                SaveSettings();
                SetTimer(hwnd, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
                ShowSoftToast(enabled ? L"Win + V abre o ClipTrace." : L"Win + V restaurado para o Windows.");
            } else {
                ShowSoftToast(L"Não foi possível ativar Win + V.");
            }
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) CloseSettingsModal(hwnd, state);
        return 0;
    case WM_CLOSE:
        CloseSettingsModal(hwnd, state);
        return 0;
    case WM_NCDESTROY:
        gActiveModalHwnd = nullptr;
        EnableWindow(state->owner, TRUE);
        ShowWindow(state->owner, SW_SHOW);
        SetForegroundWindow(state->owner);
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSettings(HWND hwnd) {
    auto* state = new SettingsDialogState{};
    state->owner = hwnd;
    state->switchToggleProgress = gStartWithWindows ? 1.0f : 0.0f;
    state->winVToggleProgress = gReplaceWinV ? 1.0f : 0.0f;

    RECT ownerRect{};
    GetWindowRect(hwnd, &ownerRect);
    constexpr int dialogWidth = 500;
    constexpr int dialogHeight = 544;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - dialogWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - dialogHeight) / 2;
    state->targetX = x;
    state->targetY = y;
    state->openProgress = 0.0f;
    state->closeProgress = 0.0f;
    state->isClosing = false;

    HWND dialog = CreateWindowExW(WS_EX_TOOLWINDOW, L"ClipTraceSettingsWindow", L"Configurações", WS_POPUP,
        x, y + 18, dialogWidth, dialogHeight, hwnd, nullptr, GetModuleHandleW(nullptr), state);
    if (!dialog) { delete state; return; }
    gActiveModalHwnd = dialog;
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(dialog, 20, &darkMode, sizeof(darkMode));
    const int roundedCorners = 2;
    DwmSetWindowAttribute(dialog, 33, &roundedCorners, sizeof(roundedCorners));
    const int micaBackdrop = 2;
    DwmSetWindowAttribute(dialog, 38, &micaBackdrop, sizeof(micaBackdrop));
    EnableWindow(hwnd, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetTimer(dialog, kSettingsAnimTimer, kSettingsAnimInterval, nullptr);
}

Gdiplus::Bitmap* LoadPngFromResource(HMODULE hMod, int resId) {
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return nullptr;
    DWORD size = SizeofResource(hMod, hRes);
    HGLOBAL hGlobal = LoadResource(hMod, hRes);
    if (!hGlobal) return nullptr;
    void* pData = LockResource(hGlobal);
    if (!pData) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) return nullptr;
    void* pDest = GlobalLock(hMem);
    if (!pDest) { GlobalFree(hMem); return nullptr; }
    memcpy(pDest, pData, size);
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)) || !pStream) {
        GlobalFree(hMem);
        return nullptr;
    }
    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromStream(pStream);
    pStream->Release();
    return bmp;
}

Gdiplus::Bitmap* LoadPngFromFileOrResource(int resId, const wchar_t* filename) {
    HMODULE hMod = GetModuleHandleW(nullptr);
    Gdiplus::Bitmap* bmp = LoadPngFromResource(hMod, resId);
    if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) return bmp;
    delete bmp;

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t pos = exeDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring candidate = exeDir.substr(0, pos + 1) + filename;
        bmp = Gdiplus::Bitmap::FromFile(candidate.c_str());
        if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) return bmp;
        delete bmp;
    }
    bmp = Gdiplus::Bitmap::FromFile(filename);
    if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) return bmp;
    delete bmp;
    return nullptr;
}

struct AboutDialogState {
    HWND owner = nullptr;
    RECT closeRect{};
    RECT devCreditRect{};
    RECT okButtonRect{};
    bool closeHovered = false;
    bool devHovered = false;
    bool okHovered = false;
    float closeHoverProgress = 0.0f;
    float devHoverProgress = 0.0f;
    float okHoverProgress = 0.0f;
    float openProgress = 0.0f;
    float closeProgress = 0.0f;
    bool isClosing = false;
    int targetX = 0;
    int targetY = 0;
};

constexpr UINT_PTR kAboutAnimTimer = 202;
constexpr UINT kAboutAnimInterval = 14;

void CloseAboutModal(HWND hwnd, AboutDialogState* state) {
    if (!state || state->isClosing) return;
    state->isClosing = true;
    state->closeProgress = 0.0f;
    SetTimer(hwnd, kAboutAnimTimer, kAboutAnimInterval, nullptr);
}

LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AboutDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<AboutDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        if (wParam != kAboutAnimTimer) break;
        bool animating = false;

        if (state->openProgress < 1.0f) {
            state->openProgress += (1.0f - state->openProgress) * 0.25f + 0.02f;
            if (state->openProgress >= 1.0f) state->openProgress = 1.0f;
            const float t = state->openProgress;
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            const int curY = state->targetY + static_cast<int>(18.0f * (1.0f - eased));
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        if (state->isClosing) {
            state->closeProgress += (1.0f - state->closeProgress) * 0.28f + 0.04f;
            if (state->closeProgress >= 1.0f) {
                KillTimer(hwnd, kAboutAnimTimer);
                DestroyWindow(hwnd);
                return 0;
            }
            const float t = state->closeProgress;
            const float eased = t * t;
            const int curY = state->targetY + static_cast<int>(18.0f * eased);
            SetWindowPos(hwnd, nullptr, state->targetX, curY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            animating = true;
        }

        constexpr float kHoverSpeed = 0.22f;
        auto stepVal = [&](float& cur, float target) {
            if (std::abs(cur - target) > 0.005f) {
                cur += (target - cur) * kHoverSpeed;
                animating = true;
            } else {
                cur = target;
            }
        };

        stepVal(state->closeHoverProgress, state->closeHovered ? 1.0f : 0.0f);
        stepVal(state->devHoverProgress, state->devHovered ? 1.0f : 0.0f);
        stepVal(state->okHoverProgress, state->okHovered ? 1.0f : 0.0f);

        InvalidateRect(hwnd, nullptr, FALSE);

        if (!animating && !state->isClosing && state->openProgress >= 1.0f) {
            KillTimer(hwnd, kAboutAnimTimer);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (IsInside(point, state->closeRect)) return HTCLIENT;
        return point.y < 44 ? HTCAPTION : HTCLIENT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC realDc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC dc = CreateCompatibleDC(realDc);
        HBITMAP memBitmap = CreateCompatibleBitmap(realDc, width, height);
        HGDIOBJ oldMemBitmap = SelectObject(dc, memBitmap);

        HBRUSH background = CreateSolidBrush(kWindowBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);

        // Header (0..42 px)
        state->closeRect = {client.right - 44, 0, client.right, 42};
        DrawTextLine(dc, ClipTraceIcons::kInfo, {18, 0, 42, 42}, gIconFont, kAccent, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"Sobre o ClipTrace  -  v1.0-preview", {46, 0, 320, 42}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (state->closeHoverProgress > 0.01f) {
            COLORREF closeBg = LerpColor(kWindowBackground, RGB(49, 62, 91), state->closeHoverProgress);
            FillRoundedRect(dc, {state->closeRect.left + 5, state->closeRect.top + 5, state->closeRect.right - 5, state->closeRect.bottom - 5}, 4, closeBg);
        }
        COLORREF closeTextColor = LerpColor(kText, RGB(255, 255, 255), state->closeHoverProgress);
        DrawTextLine(dc, ClipTraceIcons::kClose, state->closeRect, gIconFont, closeTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLine(dc, 0, 41, client.right, 41, RGB(60, 73, 101));

        Gdiplus::Graphics g(dc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        // 1. Logo no topo (proporção original preservada)
        if (!gLogoBitmap) {
            gLogoBitmap = LoadPngFromFileOrResource(IDR_LOGO, L"logo.png");
        }
        if (gLogoBitmap && gLogoBitmap->GetLastStatus() == Gdiplus::Ok) {
            constexpr int logoW = 200;
            constexpr int logoH = 58;
            int logoX = (width - logoW) / 2;
            int logoY = 50;
            g.DrawImage(gLogoBitmap, logoX, logoY, logoW, logoH);
        } else {
            DrawTextLine(dc, L"ClipTrace", {0, 50, width, 108}, gLargeIconFont, kAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 2. Abaixo da logo a descrição (FORA DE CARD, sem tags/pílulas)
        RECT descText = {26, 118, width - 26, 206};
        DrawTextLine(dc, L"ClipTrace é um gerenciador de área de transferência moderno, rápido e com foco total em privacidade para o Windows 11.\n\nDesenvolvido de forma independente e distribuído 100% gratuitamente e em código aberto para toda a comunidade.", descText, gSmallFont, RGB(190, 205, 230), DT_CENTER | DT_WORDBREAK);

        // Divisor sutil acima do rodapé
        DrawLine(dc, 20, height - 52, width - 20, height - 52, RGB(44, 56, 80));

        // 3. Rodapé canto inferior esquerdo: pequeno igual crédito, sem card no fundo
        constexpr int devW = 33;
        constexpr int devH = 22;
        const int devX = 22;
        const int devY = height - 37;

        if (!gDeveloperBitmap) {
            gDeveloperBitmap = LoadPngFromFileOrResource(IDR_DEVELOPER, L"developer.png");
        }
        if (gDeveloperBitmap && gDeveloperBitmap->GetLastStatus() == Gdiplus::Ok) {
            g.DrawImage(gDeveloperBitmap, devX, devY, devW, devH);
        }

        // Texto do Desenvolvedor ao lado do ícone
        const int textX = devX + devW + 8;
        const int textY = devY;

        SIZE sz1{}, szH{}, sz2{};
        HGDIOBJ oF = SelectObject(dc, gSmallFont);
        GetTextExtentPoint32W(dc, L"Desenvolvido com ", 17, &sz1);
        GetTextExtentPoint32W(dc, L"\x2764 ", 2, &szH);
        GetTextExtentPoint32W(dc, L"por Pablo Almeida", 17, &sz2);
        SelectObject(dc, oF);

        COLORREF textColor = LerpColor(RGB(155, 172, 200), RGB(230, 240, 255), state->devHoverProgress);
        COLORREF nameColor = LerpColor(RGB(185, 205, 238), kAccent, state->devHoverProgress);

        DrawTextLine(dc, L"Desenvolvido com ", {textX, textY, textX + sz1.cx, textY + devH}, gSmallFont, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"\x2764 ", {textX + sz1.cx, textY, textX + sz1.cx + szH.cx, textY + devH}, gSmallFont, RGB(255, 82, 102), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(dc, L"por Pablo Almeida", {textX + sz1.cx + szH.cx, textY, textX + sz1.cx + szH.cx + sz2.cx, textY + devH}, gSmallFont, nameColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Área de clique do desenvolvedor (ícone + texto)
        state->devCreditRect = {devX - 2, devY - 3, textX + sz1.cx + szH.cx + sz2.cx + 4, devY + devH + 3};

        // 4. Botão [ Fechar ] no canto inferior direito
        state->okButtonRect = {width - 104, height - 41, width - 20, height - 13};
        COLORREF okBg = LerpColor(kButtonBackground, RGB(60, 78, 116), state->okHoverProgress);
        FillRoundedRect(dc, state->okButtonRect, 6, okBg);
        COLORREF okBorder = LerpColor(RGB(55, 72, 105), RGB(90, 125, 175), state->okHoverProgress);
        HPEN okPen = CreatePen(PS_SOLID, 1, okBorder);
        HGDIOBJ oldP = SelectObject(dc, okPen); HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, state->okButtonRect.left, state->okButtonRect.top, state->okButtonRect.right, state->okButtonRect.bottom, 6, 6);
        SelectObject(dc, oldB); SelectObject(dc, oldP); DeleteObject(okPen);

        COLORREF okTextColor = LerpColor(kText, RGB(255, 255, 255), state->okHoverProgress);
        DrawTextLine(dc, L"Fechar", state->okButtonRect, gSmallFont, okTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        BitBlt(realDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldMemBitmap);
        DeleteObject(memBitmap);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool cHov = IsInside(point, state->closeRect);
        const bool dHov = IsInside(point, state->devCreditRect);
        const bool okHov = IsInside(point, state->okButtonRect);
        if (cHov != state->closeHovered || dHov != state->devHovered || okHov != state->okHovered) {
            state->closeHovered = cHov;
            state->devHovered = dHov;
            state->okHovered = okHov;
            SetTimer(hwnd, kAboutAnimTimer, kAboutAnimInterval, nullptr);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (state->closeHovered || state->devHovered || state->okHovered) {
            state->closeHovered = false;
            state->devHovered = false;
            state->okHovered = false;
            SetTimer(hwnd, kAboutAnimTimer, kAboutAnimInterval, nullptr);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        if (IsInside(point, state->closeRect) || IsInside(point, state->devCreditRect) || IsInside(point, state->okButtonRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsInside(point, state->closeRect) || IsInside(point, state->okButtonRect)) {
            CloseAboutModal(hwnd, state);
            return 0;
        }
        if (IsInside(point, state->devCreditRect)) {
            ShowSoftToast(L"Abrindo perfil do desenvolvedor no GitHub...");
            ShellExecuteW(nullptr, L"open", L"https://github.com/palmeidev/", nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) CloseAboutModal(hwnd, state);
        return 0;

    case WM_CLOSE:
        CloseAboutModal(hwnd, state);
        return 0;

    case WM_NCDESTROY:
        gActiveModalHwnd = nullptr;
        EnableWindow(state->owner, TRUE);
        ShowWindow(state->owner, SW_SHOW);
        SetForegroundWindow(state->owner);
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowAbout(HWND hwnd) {
    auto* state = new AboutDialogState{};
    state->owner = hwnd;

    RECT ownerRect{};
    GetWindowRect(hwnd, &ownerRect);
    constexpr int dialogWidth = 460;
    constexpr int dialogHeight = 276;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - dialogWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - dialogHeight) / 2;
    state->targetX = x;
    state->targetY = y;
    state->openProgress = 0.0f;
    state->closeProgress = 0.0f;
    state->isClosing = false;

    HWND dialog = CreateWindowExW(WS_EX_TOOLWINDOW, L"ClipTraceAboutWindow", L"Sobre o ClipTrace", WS_POPUP,
        x, y + 18, dialogWidth, dialogHeight, hwnd, nullptr, GetModuleHandleW(nullptr), state);
    if (!dialog) { delete state; return; }
    gActiveModalHwnd = dialog;
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(dialog, 20, &darkMode, sizeof(darkMode));
    const int roundedCorners = 2;
    DwmSetWindowAttribute(dialog, 33, &roundedCorners, sizeof(roundedCorners));
    const int micaBackdrop = 2;
    DwmSetWindowAttribute(dialog, 38, &micaBackdrop, sizeof(micaBackdrop));
    EnableWindow(hwnd, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetTimer(dialog, kAboutAnimTimer, kAboutAnimInterval, nullptr);
}

void CopyItemToClipboard(HWND hwnd, const ClipItem& item) {
    if (!OpenClipboard(hwnd)) {
        ShowSoftToast(L"Não foi possível acessar a área de transferência.");
        return;
    }
    EmptyClipboard();
    bool copied = false;
    if (item.image) {
        HGLOBAL hDib = CreateDIBFromHBitmap(item.image.get());
        if (hDib) {
            if (SetClipboardData(CF_DIB, hDib)) {
                copied = true;
            } else {
                GlobalFree(hDib);
            }
        }
        HBITMAP bitmap = static_cast<HBITMAP>(CopyImage(item.image.get(), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        if (bitmap) {
            if (SetClipboardData(CF_BITMAP, bitmap)) {
                copied = true;
            } else {
                DeleteObject(bitmap);
            }
        }
    } else if (item.type == ClipItem::Type::File && !item.rawContent.empty()) {
        std::wstring paths = item.rawContent;
        std::replace(paths.begin(), paths.end(), L'\n', L'\0');
        paths.push_back(L'\0');
        paths.push_back(L'\0');
        const SIZE_T bytes = sizeof(DROPFILES) + paths.size() * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (memory) {
            auto* drop = static_cast<DROPFILES*>(GlobalLock(memory));
            if (drop) {
                drop->pFiles = sizeof(DROPFILES);
                drop->fWide = TRUE;
                memcpy(reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES), paths.data(), paths.size() * sizeof(wchar_t));
                GlobalUnlock(memory);
                copied = SetClipboardData(CF_HDROP, memory) != nullptr;
            }
            if (!copied) GlobalFree(memory);
        }
    } else {
        const size_t bytes = (item.rawContent.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory) {
            void* destination = GlobalLock(memory);
            if (destination) { memcpy(destination, item.rawContent.c_str(), bytes); GlobalUnlock(memory); copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr; }
            if (!copied) GlobalFree(memory);
        }
    }
    CloseClipboard();
    if (copied) {
        gOurClipboardSequence = GetClipboardSequenceNumber();
        ShowSoftToast(L"Copiado com sucesso!");
    } else {
        ShowSoftToast(L"Não foi possível copiar o item.");
    }
}

bool ReadClipboard(HWND hwnd) {
    if (gMonitoringPaused) return true;
    if (!OpenClipboard(hwnd)) return false;

    const HWND targetWnd = FindTargetWindow();
    const WindowInfo winInfo = GetWindowInfo(targetWnd);
    if (winInfo.isPasswordManager) {
        CloseClipboard();
        ShowSoftToast(L"ClipTrace: Conteúdo protegido ignorado (gerenciador de senhas).");
        return true;
    }

    std::wstring pageUrl = UrlFromHtmlClipboard();
    if (pageUrl.empty() && winInfo.isBrowser && targetWnd) {
        pageUrl = GetBrowserUrlFromWindow(targetWnd);
    }
    if (pageUrl.empty()) {
        if (!winInfo.exeDir.empty()) {
            pageUrl = winInfo.exeDir;
        } else if (!winInfo.exePath.empty()) {
            pageUrl = winInfo.exePath;
        }
    }

    std::wstring source = !winInfo.processName.empty() ? winInfo.processName : L"outro aplicativo";
    if (winInfo.isBrowser) {
        if (!pageUrl.empty() && (pageUrl.rfind(L"http://", 0) == 0 || pageUrl.rfind(L"https://", 0) == 0)) {
            std::wstring site = SiteFromUrl(pageUrl);
            if (!site.empty()) {
                source = site;
            }
        }
    }

    std::wstring tabTitle = winInfo.title;
    if (winInfo.isBrowser && !tabTitle.empty()) {
        tabTitle = ExtractBrowserTabTitle(tabTitle, winInfo.processName);
    }

    ClipItem item{};
    item.source = source;
    item.pageUrl = pageUrl;
    item.pageTitle = !tabTitle.empty() ? tabTitle : winInfo.title;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(CF_HDROP) &&
        !IsClipboardFormatAvailable(CF_DIB) && !IsClipboardFormatAvailable(CF_BITMAP)) {
        HGLOBAL data = GetClipboardData(CF_UNICODETEXT);
        const wchar_t* text = data ? static_cast<const wchar_t*>(GlobalLock(data)) : nullptr;
        if (text) {
            item.rawContent = text;
            item.preview = NormalizePreview(item.rawContent);
            COLORREF colVal{};
            if (TryParseColor(item.rawContent, colVal)) {
                item.type = ClipItem::Type::Color;
            } else if (item.preview.rfind(L"http://", 0) == 0 || item.preview.rfind(L"https://", 0) == 0) {
                item.type = ClipItem::Type::Link;
            } else {
                item.type = ClipItem::Type::Text;
            }
            GlobalUnlock(data);
        }
    } else if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        const UINT count = drop ? DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) : 0;
        for (UINT index = 0; index < count; ++index) {
            const UINT length = DragQueryFileW(drop, index, nullptr, 0);
            std::wstring path(length + 1, L'\0');
            if (!DragQueryFileW(drop, index, path.data(), length + 1)) continue;
            path.resize(length);
            if (!item.rawContent.empty()) item.rawContent += L'\n';
            item.rawContent += path;
            if (item.preview.empty()) {
                item.preview = L"Arquivo: " + path;
                const size_t slash = path.find_last_of(L"\\/");
                if (slash != std::wstring::npos) item.pageUrl = path.substr(0, slash);
            }
        }
        if (count > 1 && !item.preview.empty()) item.preview = std::to_wstring(count) + L" arquivos: " + item.preview.substr(9);
        item.type = ClipItem::Type::File;
    } else if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP)) {
        item.type = ClipItem::Type::Image;
        const std::wstring captureLabel = !item.pageTitle.empty() ? item.pageTitle : item.source;
        item.preview = L"Captura de tela: " + captureLabel;
        item.rawContent = !item.pageUrl.empty() ? item.pageUrl : (L"Captura de: " + captureLabel);

        HBITMAP sourceBitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        if (sourceBitmap) {
            HBITMAP bitmapCopy = static_cast<HBITMAP>(CopyImage(sourceBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            if (bitmapCopy) item.image = std::shared_ptr<ClipItem::BitmapObject>(bitmapCopy, [](ClipItem::BitmapObject* bitmap) { DeleteObject(bitmap); });
        }
        if (!item.image && IsClipboardFormatAvailable(CF_DIB)) {
            HGLOBAL hDib = GetClipboardData(CF_DIB);
            if (hDib) {
                HBITMAP bitmapFromDib = CreateHBitmapFromDIB(hDib);
                if (bitmapFromDib) item.image = std::shared_ptr<ClipItem::BitmapObject>(bitmapFromDib, [](ClipItem::BitmapObject* bitmap) { DeleteObject(bitmap); });
            }
        }
    }
    CloseClipboard();
    if (item.preview.empty() || (item.type == ClipItem::Type::Image && !item.image)) return true;
    if (item.type == ClipItem::Type::Image && IsRepeatedClipboardImage(item.image.get(), GetTickCount64())) return true;
    AddClipboardItem(item);
    ShowCopiedNotification(item, source);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        gMainWindow = hwnd;
        gTitleFont = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Display");
        gHeaderFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Display");
        gBodyFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Text");
        gSmallFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Text");
        gIconFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe Fluent Icons");
        gLargeIconFont = CreateFontW(-56, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe Fluent Icons");
        gCodeFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        HINSTANCE hInst = GetModuleHandleW(nullptr);
        HICON hAppIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        HICON hAppIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
        gLargeAppIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
        if (!gLargeAppIcon) gLargeAppIcon = hAppIcon;
        if (hAppIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
        if (hAppIconSm) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppIconSm);

        gTrayIcon.cbSize = sizeof(gTrayIcon);
        gTrayIcon.hWnd = hwnd;
        gTrayIcon.uID = 1;
        gTrayIcon.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        gTrayIcon.uCallbackMessage = WM_TRAYICON;
        gTrayIcon.hIcon = hAppIconSm ? hAppIconSm : (hAppIcon ? hAppIcon : LoadIconW(nullptr, IDI_INFORMATION));
        wcsncpy_s(gTrayIcon.szTip, L"ClipTrace - Gerenciador de Área de Transferência", _TRUNCATE);
        Shell_NotifyIconW(NIM_ADD, &gTrayIcon);

        AddClipboardFormatListener(hwnd);
        SetTimer(hwnd, 3, 60000, nullptr);
        SetTimer(hwnd, 5, 3600000, nullptr);

        RegisterHotKey(hwnd, kGlobalHotkeyId, MOD_CONTROL | MOD_SHIFT, 'V');
        RegisterHotKey(hwnd, kGlobalHotkeyId + 1, MOD_ALT, 'V');
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_NCHITTEST: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (IsInside(point, gCloseRect)) return HTCLIENT;
        if (point.y >= 0 && point.y < kTitleBarHeight) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_HOTKEY: {
        if (wParam == kGlobalHotkeyId || wParam == kGlobalHotkeyId + 1) {
            ClipTraceUpdates::CheckAsync(hwnd);
            if (IsWindowVisible(hwnd) && !gIsClosing) {
                PostMessageW(hwnd, WM_APP + 1, 0, 0);
            } else {
                RECT workArea{};
                SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
                gTargetX = workArea.left + ((workArea.right - workArea.left) - 440) / 2;
                gTargetY = workArea.top + ((workArea.bottom - workArea.top) - 420) / 2;
                SetWindowPos(hwnd, nullptr, gTargetX, gTargetY + kAnimationOffset, 440, 420, SWP_NOZORDER);
                gIsClosing = false;
                gAnimationStep = 0;
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
                SetTimer(hwnd, 1, 12, nullptr);
            }
        }
        return 0;
    }

    case WM_OPEN_CLIPTRACE:
        ClipTraceUpdates::CheckAsync(hwnd);
        if (gActiveModalHwnd && IsWindow(gActiveModalHwnd)) {
            ShowWindow(gActiveModalHwnd, SW_SHOW);
            SetForegroundWindow(gActiveModalHwnd);
        } else {
            PostMessageW(hwnd, WM_COMMAND, IDM_TRAY_OPEN, 0);
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            if (gActiveModalHwnd && IsWindow(gActiveModalHwnd)) {
                return 0;
            }
            HWND other = (HWND)lParam;
            if (other) {
                wchar_t cls[64]{};
                GetClassNameW(other, cls, 64);
                if (wcscmp(cls, L"ClipTraceDetailWindow") == 0 ||
                    wcscmp(cls, L"ClipTraceSettingsWindow") == 0 ||
                    wcscmp(cls, L"ClipTraceAboutWindow") == 0) {
                    return 0;
                }
            }
            HWND fg = GetForegroundWindow();
            wchar_t fgClass[64]{};
            if (fg) GetClassNameW(fg, fgClass, 64);
            if (wcscmp(fgClass, L"ClipTraceDetailWindow") != 0 &&
                wcscmp(fgClass, L"ClipTraceSettingsWindow") != 0 &&
                wcscmp(fgClass, L"ClipTraceAboutWindow") != 0) {
                if (IsWindowVisible(hwnd) && !gIsClosing) {
                    PostMessageW(hwnd, WM_APP + 1, 0, 0);
                }
            }
        }
        return 0;

    case WM_TRAYICON: {
        if (lParam == NIN_BALLOONUSERCLICK && gUpdateBalloonActive) {
            gUpdateBalloonActive = false;
            PostMessageW(hwnd, WM_INSTALL_UPDATE, 0, 0);
        } else if (lParam == NIN_BALLOONHIDE || lParam == NIN_BALLOONTIMEOUT) {
            gUpdateBalloonActive = false;
        } else if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            RECT workArea{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
            gTargetX = workArea.left + ((workArea.right - workArea.left) - 440) / 2;
            gTargetY = workArea.top + ((workArea.bottom - workArea.top) - 420) / 2;
            SetWindowPos(hwnd, nullptr, gTargetX, gTargetY + kAnimationOffset, 440, 420, SWP_NOZORDER);
            gIsClosing = false;
            gAnimationStep = 0;
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            SetTimer(hwnd, 1, 12, nullptr);
        } else if (lParam == WM_RBUTTONUP) {
            POINT pt{};
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Abrir ClipTrace (Ctrl+Shift+V)");
            if (gAvailableUpdate) AppendMenuW(hMenu, MF_STRING, IDM_TRAY_UPDATE, L"Baixar atualização do ClipTrace");
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_PAUSE, gMonitoringPaused ? L"Retomar Monitoramento" : L"Pausar Monitoramento (Modo Privado)");
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLEAR, L"Limpar Histórico");
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SETTINGS, L"Configurações...");
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_ABOUT, L"Sobre o ClipTrace...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Sair do ClipTrace");
            SetMenuDefaultItem(hMenu, IDM_TRAY_OPEN, FALSE);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
        }
        return 0;
    }

    case WM_COMMAND: {
        UINT id = LOWORD(wParam);
        if (id == IDM_TRAY_OPEN) {
            ClipTraceUpdates::CheckAsync(hwnd);
            RECT workArea{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
            gTargetX = workArea.left + ((workArea.right - workArea.left) - 440) / 2;
            gTargetY = workArea.top + ((workArea.bottom - workArea.top) - 420) / 2;
            SetWindowPos(hwnd, nullptr, gTargetX, gTargetY + kAnimationOffset, 440, 420, SWP_NOZORDER);
            gIsClosing = false;
            gAnimationStep = 0;
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            SetTimer(hwnd, 1, 12, nullptr);
        } else if (id == IDM_TRAY_UPDATE) {
            PostMessageW(hwnd, WM_INSTALL_UPDATE, 0, 0);
        } else if (id == IDM_TRAY_PAUSE) {
            gMonitoringPaused = !gMonitoringPaused;
            ShowSoftToast(gMonitoringPaused ? L"Monitoramento pausado (Modo Privado)." : L"Monitoramento retomado.");
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (id == IDM_TRAY_CLEAR) {
            ClearAllHistory();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (id == IDM_TRAY_SETTINGS) {
            if (gIsClosing) { KillTimer(hwnd, 2); gIsClosing = false; }
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            ShowSettings(hwnd);
        } else if (id == IDM_TRAY_ABOUT) {
            if (gIsClosing) { KillTimer(hwnd, 2); gIsClosing = false; }
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            ShowAbout(hwnd);
        } else if (id == IDM_TRAY_EXIT) {
            UnregisterHotKey(hwnd, kGlobalHotkeyId);
            UnregisterHotKey(hwnd, kGlobalHotkeyId + 1);
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const auto filtered = GetFilteredIndices();
        const int cardH = 84;
        const int cardG = 8;
        const int totalH = static_cast<int>(filtered.size()) * (cardH + cardG);
        RECT client;
        GetClientRect(hwnd, &client);
        const int viewportH = client.bottom - 138;
        const int maxScroll = max(0, totalH - viewportH);
        gScrollOffset -= (delta / WHEEL_DELTA) * 56;
        gScrollOffset = max(0, min(gScrollOffset, maxScroll));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CHAR: {
        wchar_t ch = static_cast<wchar_t>(wParam);
        if (ch == VK_BACK) {
            if (!gSearchQuery.empty()) {
                gSearchQuery.pop_back();
                gScrollOffset = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else if (ch >= 32 && ch != 127) {
            gSearchQuery.push_back(ch);
            gScrollOffset = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (!gSearchQuery.empty()) {
                gSearchQuery.clear();
                gScrollOffset = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                PostMessageW(hwnd, WM_APP + 1, 0, 0);
            }
            return 0;
        }
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_RETURN) {
            const auto filtered = GetFilteredIndices();
            if (filtered.empty()) return 0;
            auto selected = std::find(filtered.begin(), filtered.end(), static_cast<size_t>(gSelectedIndex));
            size_t position = selected == filtered.end() ? 0 : static_cast<size_t>(selected - filtered.begin());
            if (wParam == VK_RETURN) {
                CopyItemToClipboard(hwnd, gHistory[filtered[position]]);
                return 0;
            }
            if (wParam == VK_UP && selected != filtered.end() && position > 0) --position;
            if (wParam == VK_DOWN && selected != filtered.end() && position + 1 < filtered.size()) ++position;
            gSelectedIndex = static_cast<int>(filtered[position]);
            RECT client{};
            GetClientRect(hwnd, &client);
            const int viewportHeight = client.bottom - 136;
            const int cardTop = static_cast<int>(position) * 92;
            if (cardTop < gScrollOffset) gScrollOffset = cardTop;
            if (cardTop + 84 > gScrollOffset + viewportHeight) gScrollOffset = cardTop + 84 - viewportHeight;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_MOUSEMOVE: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        bool stateChanged = false;

        const bool closeHov = IsInside(point, gCloseRect);
        if (closeHov != gCloseHovered) {
            gCloseHovered = closeHov;
            stateChanged = true;
        }

        const bool clearHov = IsInside(point, gClearRect);
        if (clearHov != gClearHovered) {
            gClearHovered = clearHov;
            stateChanged = true;
        }

        const bool pauseHov = IsInside(point, gPauseRect);
        if (pauseHov != gPauseHovered) {
            gPauseHovered = pauseHov;
            stateChanged = true;
        }

        const bool aboutHov = IsInside(point, gAboutRect);
        if (aboutHov != gAboutHovered) {
            gAboutHovered = aboutHov;
            stateChanged = true;
        }

        const bool settingsHov = IsInside(point, gSettingsRect);
        if (settingsHov != gSettingsHovered) {
            gSettingsHovered = settingsHov;
            stateChanged = true;
        }

        const bool searchHov = IsInside(point, gSearchRect);
        if (searchHov != gSearchHovered) {
            gSearchHovered = searchHov;
            stateChanged = true;
        }

        gCardButtonStates.resize(gHistory.size());
        for (const auto& hit : gCardHits) {
            if (hit.historyIndex >= gCardButtonStates.size()) continue;
            const bool iHov = IsInside(point, hit.infoRect);
            const bool pHov = IsInside(point, hit.pinRect);
            const bool cpHov = IsInside(point, hit.copyRect);
            const bool cdHov = IsInside(point, hit.bounds) && !iHov && !pHov && !cpHov;

            auto& st = gCardButtonStates[hit.historyIndex];
            if (iHov != st.infoHovered || pHov != st.pinHovered || cpHov != st.copyHovered || cdHov != st.cardHovered) {
                st.infoHovered = iHov;
                st.pinHovered = pHov;
                st.copyHovered = cpHov;
                st.cardHovered = cdHov;
                stateChanged = true;
            }
        }

        if (stateChanged) {
            SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        gCloseHovered = false;
        gClearHovered = false;
        gPauseHovered = false;
        gAboutHovered = false;
        gSettingsHovered = false;
        gSearchHovered = false;
        for (auto& st : gCardButtonStates) {
            st.infoHovered = false;
            st.pinHovered = false;
            st.copyHovered = false;
            st.cardHovered = false;
        }
        SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
        return 0;
    }

    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        if (IsInside(point, gCloseRect) || IsInside(point, gClearRect) || IsInside(point, gPauseRect) || IsInside(point, gAboutRect) || IsInside(point, gSettingsRect) || IsInside(point, gClearSearchRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        if (IsInside(point, gSearchRect)) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
            return TRUE;
        }
        for (const auto& hit : gCardHits) {
            if (IsInside(point, hit.bounds)) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    }

    case WM_SHOWWINDOW:
        if (wParam && !gIsClosing) {
            gAnimationStep = 0;
            SetTimer(hwnd, 1, 12, nullptr);
        }
        return 0;

    case WM_TIMER:
        if (wParam == 5) {
            ClipTraceUpdates::CheckAsync(hwnd, true);
            return 0;
        }
        if (wParam == 3) {
            if (ApplyAutoCleanup()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wParam == 4) {
            const DWORD seq = GetClipboardSequenceNumber();
            if (seq != gPendingClipboardSequence || seq == gLastProcessedClipboardSequence) {
                gPendingClipboardSequence = 0;
                KillTimer(hwnd, 4);
            } else if (ReadClipboard(hwnd)) {
                gLastProcessedClipboardSequence = seq;
                gPendingClipboardSequence = 0;
                KillTimer(hwnd, 4);
            } else if (++gClipboardRetryCount >= 10) {
                gPendingClipboardSequence = 0;
                KillTimer(hwnd, 4);
            }
            return 0;
        }
        if (wParam == 1) {
            ++gAnimationStep;
            const float progress = static_cast<float>(gAnimationStep) / kAnimationFrames;
            const float eased = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
            const int y = gTargetY + static_cast<int>(kAnimationOffset * (1.0f - eased));
            SetWindowPos(hwnd, nullptr, gTargetX, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            if (gAnimationStep >= kAnimationFrames) {
                SetWindowPos(hwnd, nullptr, gTargetX, gTargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                KillTimer(hwnd, 1);
            }
        }
        if (wParam == 2) {
            ++gAnimationStep;
            const float progress = static_cast<float>(gAnimationStep) / kAnimationFrames;
            const float eased = progress * progress;
            const int y = gTargetY + static_cast<int>(kAnimationOffset * eased);
            SetWindowPos(hwnd, nullptr, gTargetX, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            if (gAnimationStep >= kAnimationFrames) {
                KillTimer(hwnd, 2);
                ShowWindow(hwnd, SW_HIDE);
                gIsClosing = false;
            }
        }
        if (wParam == kMainAnimTimer) {
            bool animating = false;
            constexpr float kHoverSpeed = 0.22f;
            auto stepVal = [&](float& cur, float target) {
                if (std::abs(cur - target) > 0.005f) {
                    cur += (target - cur) * kHoverSpeed;
                    animating = true;
                } else {
                    cur = target;
                }
            };

            stepVal(gCloseHoverProgress, gCloseHovered ? 1.0f : 0.0f);
            stepVal(gClearHoverProgress, gClearHovered ? 1.0f : 0.0f);
            stepVal(gPauseHoverProgress, gPauseHovered ? 1.0f : 0.0f);
            stepVal(gAboutHoverProgress, gAboutHovered ? 1.0f : 0.0f);
            stepVal(gSettingsHoverProgress, gSettingsHovered ? 1.0f : 0.0f);
            stepVal(gSearchHoverProgress, (gSearchHovered || gSearchFocused) ? 1.0f : 0.0f);

            if (gAboutPressProgress > 0.01f) {
                gAboutPressProgress *= 0.80f;
                animating = true;
            } else {
                gAboutPressProgress = 0.0f;
            }

            if (gSettingsPressProgress > 0.01f) {
                gSettingsPressProgress *= 0.80f;
                animating = true;
            } else {
                gSettingsPressProgress = 0.0f;
            }

            if (gPausePressProgress > 0.01f) {
                gPausePressProgress *= 0.80f;
                animating = true;
            } else {
                gPausePressProgress = 0.0f;
            }

            const ULONGLONG now = GetTickCount64();
            gCardButtonStates.resize(gHistory.size());
            for (size_t i = 0; i < gCardButtonStates.size(); ++i) {
                auto& st = gCardButtonStates[i];
                stepVal(st.infoHoverProgress, st.infoHovered ? 1.0f : 0.0f);
                stepVal(st.pinHoverProgress, st.pinHovered ? 1.0f : 0.0f);
                stepVal(st.copyHoverProgress, st.copyHovered ? 1.0f : 0.0f);
                stepVal(st.cardHoverProgress, st.cardHovered ? 1.0f : 0.0f);

                if (st.infoPressProgress > 0.01f) {
                    st.infoPressProgress *= 0.80f;
                    animating = true;
                } else st.infoPressProgress = 0.0f;

                if (st.pinPressProgress > 0.01f) {
                    st.pinPressProgress *= 0.80f;
                    animating = true;
                } else st.pinPressProgress = 0.0f;

                if (st.copyPressProgress > 0.01f) {
                    st.copyPressProgress *= 0.82f;
                    animating = true;
                } else st.copyPressProgress = 0.0f;

                if (st.copiedTimestamp != 0) {
                    if (now - st.copiedTimestamp < 1000) {
                        animating = true;
                    } else {
                        st.copiedTimestamp = 0;
                        animating = true;
                    }
                }
            }

            InvalidateRect(hwnd, nullptr, FALSE);

            if (!animating) {
                KillTimer(hwnd, kMainAnimTimer);
            }
            return 0;
        }
        return 0;

    case WM_APP + 1:
        if (gActiveModalHwnd && IsWindow(gActiveModalHwnd)) {
            return 0;
        }
        if (!gIsClosing) {
            gIsClosing = true;
            gAnimationStep = 0;
            RECT currentRect{};
            GetWindowRect(hwnd, &currentRect);
            gTargetX = currentRect.left;
            gTargetY = currentRect.top;
            SetTimer(hwnd, 2, 10, nullptr);
        }
        return 0;

    case WM_CLIPBOARDUPDATE: {
        const DWORD seq = GetClipboardSequenceNumber();
        if (gOurClipboardSequence != 0 && seq == gOurClipboardSequence) {
            gOurClipboardSequence = 0;
            gLastProcessedClipboardSequence = seq;
            gPendingClipboardSequence = 0;
            KillTimer(hwnd, 4);
            return 0;
        }
        gOurClipboardSequence = 0;
        if (seq != 0 && seq == gLastProcessedClipboardSequence) {
            return 0;
        }
        if (ReadClipboard(hwnd)) {
            gLastProcessedClipboardSequence = seq;
            gPendingClipboardSequence = 0;
            KillTimer(hwnd, 4);
        } else {
            gPendingClipboardSequence = seq;
            gClipboardRetryCount = 0;
            SetTimer(hwnd, 4, 100, nullptr);
        }
        return 0;
    }

    case ClipTraceUpdates::AvailableMessage: {
        std::unique_ptr<ClipTraceUpdates::Release> release(reinterpret_cast<ClipTraceUpdates::Release*>(lParam));
        if (release && (!gAvailableUpdate || gAvailableUpdate->tag != release->tag)) {
            gAvailableUpdate = std::move(release);
            ShowUpdateNotification(hwnd);
        }
        return 0;
    }

    case WM_INSTALL_UPDATE:
        if (gAvailableUpdate && ClipTraceUpdates::InstallAsync(hwnd, *gAvailableUpdate)) {
            ShowSoftToast(L"Baixando e verificando a atualização...");
        }
        return 0;

    case ClipTraceUpdates::ErrorMessage: {
        std::unique_ptr<std::wstring> detail(reinterpret_cast<std::wstring*>(lParam));
        if (detail) MessageBoxW(hwnd, detail->c_str(), L"Atualização do ClipTrace", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 0;
    }

    case ClipTraceUpdates::ReadyMessage:
        DestroyWindow(hwnd);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC realDc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC dc = CreateCompatibleDC(realDc);
        HBITMAP memBmp = CreateCompatibleBitmap(realDc, width, height);
        HGDIOBJ oldMemBmp = SelectObject(dc, memBmp);

        HBRUSH background = CreateSolidBrush(kWindowBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);

        // --- Title Bar ---
        gCloseRect = {client.right - kCloseWidth, 0, client.right, kTitleBarHeight};
        HICON hSmIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
        if (hSmIcon) {
            DrawIconEx(dc, 12, (kTitleBarHeight - 24) / 2, hSmIcon, 24, 24, 0, nullptr, DI_NORMAL);
            DrawTextLine(dc, L"ClipTrace", {44, 0, 190, kTitleBarHeight}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            DrawTextLine(dc, L"ClipTrace", {16, 0, 190, kTitleBarHeight}, gTitleFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        if (gCloseHoverProgress > 0.01f) {
            COLORREF closeBg = LerpColor(kWindowBackground, RGB(49, 62, 91), gCloseHoverProgress);
            FillRoundedRect(dc, {gCloseRect.left + 5, gCloseRect.top + 4, gCloseRect.right - 5, gCloseRect.bottom - 4}, 4, closeBg);
        }
        COLORREF closeTextColor = LerpColor(kText, RGB(255, 255, 255), gCloseHoverProgress);
        DrawTextLine(dc, ClipTraceIcons::kClose, gCloseRect, gIconFont, closeTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawLine(dc, 0, kTitleBarHeight - 1, client.right, kTitleBarHeight - 1, RGB(60, 73, 101));

        // --- Action Header ---
        DrawTextLine(dc, L"ÁREA DE TRANSFERÊNCIA", {20, 54, client.right - 212, 80}, gHeaderFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawLine(dc, 20, 85, 197, 85, kAccent, 3);

        // Settings Button (28x28)
        gSettingsRect = {client.right - 46, 52, client.right - 20, 78};
        COLORREF settingsBg = LerpColor(kButtonBackground, RGB(60, 76, 112), gSettingsHoverProgress);
        if (gSettingsPressProgress > 0.01f) settingsBg = LerpColor(settingsBg, RGB(35, 80, 140), gSettingsPressProgress);
        FillRoundedRect(dc, gSettingsRect, 6, settingsBg);
        if (gSettingsHoverProgress > 0.01f || gSettingsPressProgress > 0.01f) {
            float sActive = max(gSettingsHoverProgress, gSettingsPressProgress);
            HPEN sPen = CreatePen(PS_SOLID, 1, LerpColor(kButtonBackground, RGB(90, 120, 170), sActive));
            HGDIOBJ oP = SelectObject(dc, sPen); HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, gSettingsRect.left, gSettingsRect.top, gSettingsRect.right, gSettingsRect.bottom, 6, 6);
            SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(sPen);
        }
        COLORREF settingsIconColor = LerpColor(kText, RGB(255, 255, 255), max(gSettingsHoverProgress, gSettingsPressProgress));
        DrawTextLine(dc, ClipTraceIcons::kSettings, gSettingsRect, gIconFont, settingsIconColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // About Button (28x28) with 'i' (kInfo)
        gAboutRect = {client.right - 78, 52, client.right - 52, 78};
        COLORREF aboutBg = LerpColor(kButtonBackground, RGB(60, 76, 112), gAboutHoverProgress);
        if (gAboutPressProgress > 0.01f) aboutBg = LerpColor(aboutBg, RGB(35, 80, 140), gAboutPressProgress);
        FillRoundedRect(dc, gAboutRect, 6, aboutBg);
        if (gAboutHoverProgress > 0.01f || gAboutPressProgress > 0.01f) {
            float aActive = max(gAboutHoverProgress, gAboutPressProgress);
            HPEN aPen = CreatePen(PS_SOLID, 1, LerpColor(kButtonBackground, RGB(90, 120, 170), aActive));
            HGDIOBJ oPa = SelectObject(dc, aPen); HGDIOBJ oBa = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, gAboutRect.left, gAboutRect.top, gAboutRect.right, gAboutRect.bottom, 6, 6);
            SelectObject(dc, oBa); SelectObject(dc, oPa); DeleteObject(aPen);
        }
        COLORREF aboutIconColor = LerpColor(kText, RGB(255, 255, 255), max(gAboutHoverProgress, gAboutPressProgress));
        DrawTextLine(dc, ClipTraceIcons::kInfo, gAboutRect, gIconFont, aboutIconColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Pause/Play Button (28x28)
        gPauseRect = {client.right - 110, 52, client.right - 84, 78};
        COLORREF pauseBg = gMonitoringPaused ? RGB(60, 48, 20) : LerpColor(kButtonBackground, RGB(60, 76, 112), gPauseHoverProgress);
        if (gPausePressProgress > 0.01f) pauseBg = LerpColor(pauseBg, RGB(35, 80, 140), gPausePressProgress);
        FillRoundedRect(dc, gPauseRect, 6, pauseBg);
        COLORREF pauseBorder = gMonitoringPaused ? RGB(255, 185, 0) : LerpColor(kButtonBackground, RGB(90, 120, 170), max(gPauseHoverProgress, gPausePressProgress));
        HPEN pPen = CreatePen(PS_SOLID, 1, pauseBorder);
        HGDIOBJ oPp = SelectObject(dc, pPen); HGDIOBJ oPb = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, gPauseRect.left, gPauseRect.top, gPauseRect.right, gPauseRect.bottom, 6, 6);
        SelectObject(dc, oPb); SelectObject(dc, oPp); DeleteObject(pPen);
        COLORREF pauseColor = gMonitoringPaused ? RGB(255, 185, 0) : LerpColor(kText, RGB(255, 255, 255), max(gPauseHoverProgress, gPausePressProgress));
        const wchar_t* pauseIcon = gMonitoringPaused ? ClipTraceIcons::kPlay : ClipTraceIcons::kPause;
        DrawTextLine(dc, pauseIcon, gPauseRect, gIconFont, pauseColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Clear All Button
        gClearRect = {client.right - 206, 52, client.right - 116, 78};
        COLORREF clearBg = LerpColor(kButtonBackground, RGB(60, 76, 112), gClearHoverProgress);
        FillRoundedRect(dc, gClearRect, 6, clearBg);
        if (gClearHoverProgress > 0.01f) {
            HPEN cPen = CreatePen(PS_SOLID, 1, LerpColor(kButtonBackground, RGB(90, 120, 170), gClearHoverProgress));
            HGDIOBJ oP = SelectObject(dc, cPen); HGDIOBJ oB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, gClearRect.left, gClearRect.top, gClearRect.right, gClearRect.bottom, 6, 6);
            SelectObject(dc, oB); SelectObject(dc, oP); DeleteObject(cPen);
        }
        COLORREF clearTextColor = LerpColor(kText, RGB(255, 255, 255), gClearHoverProgress);
        DrawTextLine(dc, L"Limpar tudo", gClearRect, gSmallFont, clearTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // --- Search Bar (20 to client.right - 20) ---
        gSearchRect = {20, 92, client.right - 20, 122};
        COLORREF searchBg = RGB(33, 44, 68);
        FillRoundedRect(dc, gSearchRect, 6, searchBg);
        COLORREF searchBorder = gSearchFocused ? kAccent : LerpColor(RGB(48, 62, 88), RGB(72, 92, 128), gSearchHoverProgress);
        HPEN sBorderPen = CreatePen(PS_SOLID, 1, searchBorder);
        HGDIOBJ osP = SelectObject(dc, sBorderPen); HGDIOBJ osB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, gSearchRect.left, gSearchRect.top, gSearchRect.right, gSearchRect.bottom, 6, 6);
        SelectObject(dc, osB); SelectObject(dc, osP); DeleteObject(sBorderPen);

        RECT searchIconRect = {gSearchRect.left + 8, gSearchRect.top, gSearchRect.left + 26, gSearchRect.bottom};
        DrawTextLine(dc, ClipTraceIcons::kSearch, searchIconRect, gIconFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        gClearSearchRect = {gSearchRect.right - 26, gSearchRect.top + 3, gSearchRect.right - 4, gSearchRect.bottom - 3};
        RECT searchTextRect = {gSearchRect.left + 30, gSearchRect.top, gSearchRect.right - 30, gSearchRect.bottom};
        if (gSearchQuery.empty()) {
            DrawTextLine(dc, L"Pesquisar no histórico...", searchTextRect, gSmallFont, RGB(140, 155, 185), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            DrawTextLine(dc, gSearchQuery.c_str(), searchTextRect, gBodyFont, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            DrawTextLine(dc, ClipTraceIcons::kCancel, gClearSearchRect, gIconFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // --- Cards Viewport with Clipping ---
        const int viewportTop = 130;
        const int viewportBottom = client.bottom - 6;
        const int viewportHeight = viewportBottom - viewportTop;

        const auto filteredIndices = GetFilteredIndices();
        gCardButtonStates.resize(gHistory.size());
        gCardHits.clear();

        if (filteredIndices.empty()) {
            const int centerX = client.right / 2;
            if (gSearchQuery.empty()) {
                DrawTextLine(dc, ClipTraceIcons::kEmptyClipboard, {centerX - 40, 165, centerX + 40, 235}, gLargeIconFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(dc, L"Nenhum item copiado ainda", {20, 248, client.right - 20, 273}, gTitleFont, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(dc, L"Copie texto, links, imagens ou arquivos para vê-los aqui.", {20, 273, client.right - 20, 297}, gSmallFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                DrawTextLine(dc, ClipTraceIcons::kSearch, {centerX - 40, 165, centerX + 40, 235}, gLargeIconFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(dc, L"Nenhum resultado encontrado", {20, 248, client.right - 20, 273}, gTitleFont, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(dc, L"Tente buscar por outras palavras-chave ou limpe a busca.", {20, 273, client.right - 20, 297}, gSmallFont, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        } else {
            const int cardHeight = 84;
            const int cardGap = 8;
            const int totalContentHeight = static_cast<int>(filteredIndices.size()) * (cardHeight + cardGap);
            const int maxScroll = max(0, totalContentHeight - viewportHeight);
            gScrollOffset = max(0, min(gScrollOffset, maxScroll));

            HRGN hClipRgn = CreateRectRgn(20, viewportTop, client.right - 10, viewportBottom);
            SelectClipRgn(dc, hClipRgn);

            for (size_t k = 0; k < filteredIndices.size(); ++k) {
                size_t histIdx = filteredIndices[k];
                int top = viewportTop - gScrollOffset + static_cast<int>(k) * (cardHeight + cardGap);
                int bottom = top + cardHeight;
                if (bottom < viewportTop || top > viewportBottom) continue;

                RECT cardRect = {20, top, client.right - 20, bottom};
                RECT infoR{}, pinR{}, copyR{};
                DrawCard(dc, cardRect, gHistory[histIdx], static_cast<int>(histIdx) == gSelectedIndex, gCardButtonStates[histIdx], infoR, pinR, copyR);
                gCardHits.push_back({cardRect, infoR, pinR, copyR, histIdx});
            }

            SelectClipRgn(dc, nullptr);
            DeleteObject(hClipRgn);

            // Scrollbar
            if (totalContentHeight > viewportHeight && maxScroll > 0) {
                int thumbH = max(24, (viewportHeight * viewportHeight) / totalContentHeight);
                int thumbY = viewportTop + (gScrollOffset * (viewportHeight - thumbH)) / maxScroll;
                RECT thumbRect = {client.right - 8, thumbY, client.right - 4, thumbY + thumbH};
                FillRoundedRect(dc, thumbRect, 2, RGB(80, 96, 128));
            }
        }

        BitBlt(realDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldMemBmp);
        DeleteObject(memBmp);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsInside(point, gSettingsRect)) {
            gSettingsPressProgress = 1.0f;
            SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (IsInside(point, gAboutRect)) {
            gAboutPressProgress = 1.0f;
            SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (IsInside(point, gPauseRect)) {
            gPausePressProgress = 1.0f;
            SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        for (const auto& hit : gCardHits) {
            if (hit.historyIndex >= gCardButtonStates.size()) continue;
            if (IsInside(point, hit.infoRect)) {
                gCardButtonStates[hit.historyIndex].infoPressProgress = 1.0f;
                SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (IsInside(point, hit.pinRect)) {
                gCardButtonStates[hit.historyIndex].pinPressProgress = 1.0f;
                SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (IsInside(point, hit.copyRect)) {
                gCardButtonStates[hit.historyIndex].copyPressProgress = 1.0f;
                SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsInside(point, gCloseRect)) {
            PostMessageW(hwnd, WM_APP + 1, 0, 0);
        } else if (IsInside(point, gSettingsRect)) {
            if (gIsClosing) { KillTimer(hwnd, 2); gIsClosing = false; SetWindowPos(hwnd, nullptr, gTargetX, gTargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }
            gSettingsPressProgress = 1.0f;
            ShowSettings(hwnd);
        } else if (IsInside(point, gAboutRect)) {
            if (gIsClosing) { KillTimer(hwnd, 2); gIsClosing = false; SetWindowPos(hwnd, nullptr, gTargetX, gTargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }
            gAboutPressProgress = 1.0f;
            ShowAbout(hwnd);
        } else if (IsInside(point, gPauseRect)) {
            gPausePressProgress = 1.0f;
            gMonitoringPaused = !gMonitoringPaused;
            ShowSoftToast(gMonitoringPaused ? L"Monitoramento pausado (Modo Privado)." : L"Monitoramento retomado.");
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (IsInside(point, gClearRect)) {
            ClearAllHistory();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (!gSearchQuery.empty() && IsInside(point, gClearSearchRect)) {
            gSearchQuery.clear();
            gScrollOffset = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (IsInside(point, gSearchRect)) {
            gSearchFocused = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else {
            gSearchFocused = false;
            for (const auto& hit : gCardHits) {
                if (!IsInside(point, hit.bounds)) continue;
                gSelectedIndex = static_cast<int>(hit.historyIndex);
                if (IsInside(point, hit.infoRect)) {
                    if (gIsClosing) { KillTimer(hwnd, 2); gIsClosing = false; SetWindowPos(hwnd, nullptr, gTargetX, gTargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }
                    gCardButtonStates[hit.historyIndex].infoPressProgress = 1.0f;
                    ShowDetails(hwnd, gHistory[hit.historyIndex]);
                } else if (IsInside(point, hit.pinRect)) {
                    gCardButtonStates[hit.historyIndex].pinPressProgress = 1.0f;
                    ToggleItemPinned(hit.historyIndex);
                } else {
                    gCardButtonStates[hit.historyIndex].copiedTimestamp = GetTickCount64();
                    gCardButtonStates[hit.historyIndex].copyPressProgress = 1.0f;
                    CopyItemToClipboard(hwnd, gHistory[hit.historyIndex]);
                }
                SetTimer(hwnd, kMainAnimTimer, kMainAnimInterval, nullptr);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            }
        }
        return 0;
    }

    case WM_DESTROY:
        gUpdateToast = nullptr;
        SetWinVReplacement(false);
        gMainWindow = nullptr;
        KillTimer(hwnd, 3);
        KillTimer(hwnd, 5);
        UnregisterHotKey(hwnd, kGlobalHotkeyId);
        UnregisterHotKey(hwnd, kGlobalHotkeyId + 1);
        SaveHistoryToDisk();
        SaveSettings();
        RemoveClipboardFormatListener(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &gTrayIcon);
        DeleteObject(gTitleFont); DeleteObject(gHeaderFont); DeleteObject(gBodyFont); DeleteObject(gSmallFont); DeleteObject(gIconFont); DeleteObject(gLargeIconFont); DeleteObject(gCodeFont);
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        PostMessageW(hwnd, WM_APP + 1, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    int helperExitCode = 0;
    if (ClipTraceUpdates::RunHelperIfRequested(helperExitCode)) return helperExitCode;
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\ClipTrace.SingleInstance");
    if (!instanceMutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        for (int attempt = 0; attempt < 40; ++attempt) {
            HWND existing = FindWindowW(L"ClipTraceWindow", nullptr);
            if (existing) {
                SetForegroundWindow(existing);
                PostMessageW(existing, WM_OPEN_CLIPTRACE, 0, 0);
                break;
            }
            Sleep(50);
        }
        CloseHandle(instanceMutex);
        return 0;
    }
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    HICON hAppIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    HICON hAppIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    RegisterAppUserModelId(hAppIcon);

    constexpr wchar_t kWindowClass[] = L"ClipTraceWindow";
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WndProc;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = hAppIcon;
    RegisterClassW(&windowClass);

    WNDCLASSW detailClass{};
    detailClass.hInstance = instance;
    detailClass.lpfnWndProc = DetailWndProc;
    detailClass.lpszClassName = L"ClipTraceDetailWindow";
    detailClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&detailClass);

    WNDCLASSW settingsClass{};
    settingsClass.hInstance = instance;
    settingsClass.lpfnWndProc = SettingsWndProc;
    settingsClass.lpszClassName = L"ClipTraceSettingsWindow";
    settingsClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&settingsClass);

    WNDCLASSW aboutClass{};
    aboutClass.hInstance = instance;
    aboutClass.lpfnWndProc = AboutWndProc;
    aboutClass.lpszClassName = L"ClipTraceAboutWindow";
    aboutClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&aboutClass);

    LoadSettings();
    if (gStartWithWindows) {
        SetAutoStartEnabled(true);
    }
    LoadHistoryFromDisk();
    ApplyAutoCleanup();

    constexpr int kWindowWidth = 440;
    constexpr int kWindowHeight = 420;
    HWND hwnd = CreateWindowExW(0, kWindowClass, L"ClipTrace", WS_POPUP,
        0, 0, kWindowWidth, kWindowHeight, nullptr, nullptr, instance, nullptr);

    if (hAppIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
    if (hAppIconSm) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppIconSm);

    if (gReplaceWinV && !SetWinVReplacement(true)) {
        gReplaceWinV = false;
        SaveSettings();
    }

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    const int roundedCorners = 2;
    DwmSetWindowAttribute(hwnd, 33, &roundedCorners, sizeof(roundedCorners));
    const int micaBackdrop = 2;
    DwmSetWindowAttribute(hwnd, 38, &micaBackdrop, sizeof(micaBackdrop));

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    gTargetX = workArea.left + ((workArea.right - workArea.left) - kWindowWidth) / 2;
    gTargetY = workArea.top + ((workArea.bottom - workArea.top) - kWindowHeight) / 2;
    SetWindowPos(hwnd, nullptr, gTargetX, gTargetY + kAnimationOffset, kWindowWidth, kWindowHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    ShowWindow(hwnd, show); UpdateWindow(hwnd);
    ClipTraceUpdates::CheckAsync(hwnd, true);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    if (gLogoBitmap) { delete gLogoBitmap; gLogoBitmap = nullptr; }
    if (gDeveloperBitmap) { delete gDeveloperBitmap; gDeveloperBitmap = nullptr; }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    winrt::uninit_apartment();

    ReleaseMutex(instanceMutex);
    CloseHandle(instanceMutex);

    return 0;
}

