// hero_overlay_c++.cpp : Win32 port of tools/hero_overlay.py.

#include "framework.h"
#include "hero_overlay_c++.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

namespace
{
constexpr int kTimerId = 1;
constexpr int kTimerMs = 100;
constexpr COLORREF kTransparentColor = RGB(0, 0, 2);
constexpr COLORREF kControlBg = RGB(30, 30, 46);
constexpr COLORREF kLogBg = RGB(17, 17, 27);
constexpr COLORREF kLogFg = RGB(166, 173, 200);
constexpr COLORREF kBarBg = RGB(49, 50, 68);
constexpr COLORREF kBarFg = RGB(249, 226, 175);
constexpr int kCanvasWidth = 360;
constexpr int kCanvasHeight = 450;
constexpr int kIndicatorWidth = 4;
constexpr int kIndicatorHeight = 60;
constexpr int kHorizontalHeight = 18;
constexpr int kHorizontalGap = 3;
constexpr int kHorizontalPaddingX = 4;
constexpr int kHorizontalYOffset = 32;
constexpr int kLevelTextYOffset = 10;
constexpr int kLevelTextHeight = 24;
constexpr int kLevelFontHeight = 22;
constexpr int kHorizontalBlockBaseWidth = 12;
constexpr int kHorizontalReducedBlockBaseWidth = 10;
constexpr int kHorizontalBlockMinWidth = 4;
constexpr int kVerticalLevelPaddingX = 4;
constexpr COLORREF kBlockEmpty = RGB(120, 101, 58);
constexpr COLORREF kBlockFilled = RGB(255, 219, 88);
constexpr COLORREF kBlockOutline = RGB(255, 232, 120);
constexpr COLORREF kLevelTextColor = RGB(255, 232, 120);

constexpr int IDC_LOG = 1001;
constexpr int IDC_OFFSET_X = 1002;
constexpr int IDC_OFFSET_Y = 1003;
constexpr int IDC_GAP_Y = 1004;
constexpr int IDC_APPLY = 1005;
constexpr int IDC_TEST = 1006;
constexpr int IDC_EXIT_OVERLAY = 1007;
constexpr int IDC_EXP_MODE = 1008;
constexpr int IDC_NUMBER_STYLE = 1009;
constexpr int IDC_LEVEL_COLORS = 1010;
constexpr int IDC_COLOR_LOW = 1011;
constexpr int IDC_COLOR_WHITE = 1012;
constexpr int IDC_COLOR_BROWN = 1013;
constexpr int IDC_COLOR_GOLD = 1014;
constexpr int IDC_SKILL_POINTS = 1015;
constexpr int kLevelFlashMs = 800;
constexpr bool kVerboseLogging = false;

enum class ExperienceMode
{
    Vertical,
    HorizontalBlocks,
    HorizontalReduced,
    HorizontalBars,
};

enum class NumberStyle
{
    Arabic,
    Roman,
};

enum class SkillPointsStyle
{
    ShowAll,
    ShowIndicator,
};

std::wstring IntToWide(int value)
{
    return std::to_wstring(value);
}

std::wstring ColorToHex(COLORREF color)
{
    wchar_t buffer[16] = {};
    std::swprintf(
        buffer,
        static_cast<size_t>(std::size(buffer)),
        L"#%02X%02X%02X",
        GetRValue(color),
        GetGValue(color),
        GetBValue(color));
    return buffer;
}

bool TryParseHexColor(const std::wstring& text, COLORREF& out)
{
    std::wstring value;
    for (wchar_t ch : text)
    {
        if (!iswspace(ch))
        {
            value.push_back(ch);
        }
    }

    if (!value.empty() && value[0] == L'#')
    {
        value.erase(value.begin());
    }
    if (value.size() != 6)
    {
        return false;
    }

    wchar_t* end = nullptr;
    unsigned long rgb = wcstoul(value.c_str(), &end, 16);
    if (end == value.c_str() || *end != L'\0' || rgb > 0xFFFFFF)
    {
        return false;
    }

    out = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    return true;
}

std::wstring ToWide(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (required <= 0)
    {
        required = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
        if (required <= 0)
        {
            return L"";
        }
        std::wstring out(static_cast<size_t>(required - 1), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, out.data(), required);
        return out;
    }

    std::wstring out(static_cast<size_t>(required - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), required);
    return out;
}

std::string ToNarrowLossy(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0)
    {
        return "";
    }

    std::string out(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), required, nullptr, nullptr);
    return out;
}

std::wstring LowerCopy(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

std::string LowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return value;
}

std::string Hex32(uint32_t value)
{
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return buffer;
}

struct HeroInfo
{
    int pct = -1;
    int level = 1;
    int xpInLevel = 0;
    int xpNeeded = 1;
    int availableSkillPoints = 0;
    std::string name;
};

struct SlotInfo
{
    int pct = -1;
    int level = 1;
    int xpInLevel = 0;
    int xpNeeded = 1;
    int availableSkillPoints = 0;
    std::string label = "Empty";
};

struct LevelInfo
{
    int pct = 0;
    int level = 1;
    int xpInLevel = 0;
    int xpNeeded = 1;
};

struct TacticalMapState
{
    uint32_t viewCtrl = 0;
    int32_t flag = 0;
    bool open = false;
};

struct GameProcess
{
    HANDLE process = nullptr;
    HWND hwnd = nullptr;
    std::wstring title;
};

struct WindowInfo
{
    HWND hwnd = nullptr;
    DWORD pid = 0;
    std::wstring title;
};

BOOL CALLBACK EnumVisibleWindows(HWND hwnd, LPARAM lParam)
{
    auto* windows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    if (!IsWindowVisible(hwnd))
    {
        return TRUE;
    }

    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    windows->push_back(WindowInfo{ hwnd, pid, title });
    return TRUE;
}

bool QueryProcessImagePath(DWORD pid, std::wstring& imagePath, DWORD& lastError)
{
    lastError = 0;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        lastError = GetLastError();
        return false;
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    BOOL ok = QueryFullProcessImageNameW(process, 0, buffer, &size);
    if (ok)
    {
        imagePath.assign(buffer, size);
    }
    else
    {
        lastError = GetLastError();
    }

    CloseHandle(process);
    return ok == TRUE;
}

GameProcess FindGameProcess(void (*logFn)(const std::string&))
{
    std::vector<WindowInfo> windows;
    EnumWindows(EnumVisibleWindows, reinterpret_cast<LPARAM>(&windows));

    std::vector<const WindowInfo*> candidates;
    for (const WindowInfo& window : windows)
    {
        std::wstring titleLower = LowerCopy(window.title);
        if (titleLower.find(L"imperivm") != std::wstring::npos ||
            titleLower.find(L"gbr") != std::wstring::npos ||
            titleLower.find(L"celtic") != std::wstring::npos ||
            titleLower.find(L"battles") != std::wstring::npos)
        {
            candidates.push_back(&window);

            if (kVerboseLogging)
            {
                std::ostringstream msg;
                msg << "[DEBUG] Found potential game window: '" << ToNarrowLossy(window.title)
                    << "' (PID: " << window.pid << ", HWND: 0x" << std::hex
                    << reinterpret_cast<uintptr_t>(window.hwnd) << std::dec << ")";
                logFn(msg.str());
            }
        }
    }

    for (const WindowInfo& window : windows)
    {
        if (std::find(candidates.begin(), candidates.end(), &window) == candidates.end())
        {
            candidates.push_back(&window);
        }
    }

    for (const WindowInfo* window : candidates)
    {
        DWORD error = 0;
        std::wstring imagePath;
        if (!QueryProcessImagePath(window->pid, imagePath, error))
        {
            if (kVerboseLogging && !window->title.empty())
            {
                std::ostringstream msg;
                msg << "[DEBUG] Query process image failed for PID " << window->pid
                    << " with error code: " << error;
                logFn(msg.str());
            }
            continue;
        }

        std::wstring lowerPath = LowerCopy(imagePath);
        if (lowerPath.find(L"gbr.exe") == std::wstring::npos &&
            lowerPath.find(L"gbr_custom.exe") == std::wstring::npos)
        {
            continue;
        }

        if (kVerboseLogging)
        {
            logFn("[DEBUG] Process image path: " + ToNarrowLossy(imagePath));
        }
        HANDLE readHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, window->pid);
        if (!readHandle)
        {
            DWORD readError = GetLastError();
            std::ostringstream msg;
            msg << "Cannot read game process memory (error " << readError
                << "). Try running the overlay as Administrator if the game is elevated.";
            logFn(msg.str());
            continue;
        }

        if (kVerboseLogging)
        {
            logFn("[DEBUG] Successfully opened process handle for memory reading.");
        }
        return GameProcess{ readHandle, window->hwnd, window->title };
    }

    return {};
}

class GameMemoryReader
{
public:
    using LogFn = void (*)(const std::string&);

    GameMemoryReader(HANDLE process, LogFn logFn) : process_(process), logFn_(logFn)
    {
        ReadExpTable();
    }

    bool IsValidPtr(uint32_t ptr) const
    {
        return ptr > 0x10000 && ptr < 0x7FFFFFFF;
    }

    bool ReadBytes(uint32_t address, void* out, size_t size) const
    {
        if (!IsValidPtr(address) || out == nullptr || size == 0)
        {
            return false;
        }

        SIZE_T bytesRead = 0;
        BOOL ok = ReadProcessMemory(
            process_,
            reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),
            out,
            size,
            &bytesRead);

        return ok && bytesRead == size;
    }

    int32_t ReadInt(uint32_t address) const
    {
        int32_t value = 0;
        ReadBytes(address, &value, sizeof(value));
        return value;
    }

    uint32_t ReadPtr(uint32_t address) const
    {
        uint32_t value = 0;
        ReadBytes(address, &value, sizeof(value));
        return value;
    }

    TacticalMapState ReadTacticalMapState() const
    {
        TacticalMapState state;
        state.viewCtrl = ReadPtr(0x009C0938);
        if (!IsValidPtr(state.viewCtrl))
        {
            return state;
        }

        // [view_ctrl + 0x0C]: 1 = tactical overview map open, 0 = ground view.
        state.flag = ReadInt(state.viewCtrl + 0x0C);
        state.open = state.flag == 1;
        return state;
    }

    std::array<SlotInfo, 5> ScanHeroSlots() const
    {
        std::array<SlotInfo, 5> slots;

        uint32_t game = ReadPtr(0x00996ff4);
        if (!IsValidPtr(game))
        {
            return slots;
        }

        uint32_t player = ReadPtr(game + 0x1514);
        if (!IsValidPtr(player))
        {
            return slots;
        }

        for (int hotkey = 1; hotkey <= 5; ++hotkey)
        {
            uint32_t slotAddr = player + 0x0AC + static_cast<uint32_t>(hotkey * 0x2C);
            std::vector<uint16_t> uids = ReadListUnits(slotAddr + 0x14);
            for (uint16_t uid : uids)
            {
                HeroInfo hero;
                if (ReadHeroFromUid(uid, hero))
                {
                    slots[static_cast<size_t>(hotkey - 1)].pct = hero.pct;
                    slots[static_cast<size_t>(hotkey - 1)].level = hero.level;
                    slots[static_cast<size_t>(hotkey - 1)].xpInLevel = hero.xpInLevel;
                    slots[static_cast<size_t>(hotkey - 1)].xpNeeded = hero.xpNeeded;
                    slots[static_cast<size_t>(hotkey - 1)].availableSkillPoints = hero.availableSkillPoints;
                    slots[static_cast<size_t>(hotkey - 1)].label = hero.name.empty() ? "Hero" : hero.name;
                    break;
                }
            }
        }

        return slots;
    }

private:
    bool ReadHeroFromUid(uint16_t uid, HeroInfo& hero) const
    {
        constexpr uint32_t registryAddr = 0x00a2a610;
        uint32_t unit = ReadPtr(registryAddr + static_cast<uint32_t>(uid) * 4);
        if (unit == 0)
        {
            return false;
        }

        uint32_t unitTemplate = ReadPtr(unit + 0x3c);
        if (!InheritsFromHero(unitTemplate))
        {
            return false;
        }

        std::string commandState = GetStdString(unit + 0x110);
        if (commandState == "die")
        {
            return false;
        }

        std::string customName = GetStdString(unit + 0xa0);
        std::string lowerName = LowerCopy(customName);
        if (lowerName.find("tomb") != std::string::npos ||
            lowerName.find("tumba") != std::string::npos ||
            lowerName.find("corpse") != std::string::npos)
        {
            return false;
        }

        int32_t xp = ReadInt(unit + 0x180);
        LevelInfo level = GetLevelInfoFromXp(xp);
        hero = HeroInfo{
            level.pct,
            level.level,
            level.xpInLevel,
            level.xpNeeded,
            ComputeAvailableSkillPoints(unit, level),
            customName.empty() ? "Hero" : customName
        };
        return true;
    }

    int ComputeAvailableSkillPoints(uint32_t unit, const LevelInfo& level) const
    {
        std::array<int8_t, 25> skills = {};
        if (!ReadBytes(unit + 0x1FC, skills.data(), skills.size()))
        {
            return 0;
        }

        int totalSpent = 0;
        int skillSlots = 0;
        for (int8_t skillLevel : skills)
        {
            if (skillLevel >= 0)
            {
                totalSpent += skillLevel;
                ++skillSlots;
            }
        }

        // Heroes start at visible level 1 with one skill point already available.
        int earnedPoints = level.level + 1;
        int available = max(0, earnedPoints - totalSpent);
        int remainingCapacity = max(0, skillSlots * 10 - totalSpent);
        return std::clamp(available, 0, remainingCapacity);
    }

    void Log(const std::string& message) const
    {
        if (logFn_)
        {
            logFn_(message);
        }
    }

    void ReadExpTable()
    {
        constexpr uint32_t expTableAddr = 0x009bf880;
        std::array<int32_t, 1000> table = {};
        if (ReadBytes(expTableAddr, table.data(), table.size() * sizeof(int32_t)))
        {
            expTable_.assign(table.begin(), table.end());
            if (kVerboseLogging)
            {
                Log("Loaded experience table from memory.");
            }
        }
        else
        {
            Log("Warning: Failed to load experience table from memory address 0x009bf880.");
        }
    }

    LevelInfo GetLevelInfoFromXp(int32_t xp) const
    {
        if (expTable_.empty())
        {
            return {};
        }

        size_t level = 1;
        for (size_t i = 1; i < expTable_.size(); ++i)
        {
            if (expTable_[i] > xp)
            {
                level = i - 1;
                break;
            }

            level = i;
        }

        int32_t currentLevelXp = expTable_[level];
        int32_t nextLevelXp = level + 1 < expTable_.size() ? expTable_[level + 1] : currentLevelXp;
        int32_t xpNeeded = nextLevelXp - currentLevelXp;
        if (xpNeeded == 0)
        {
            xpNeeded = 1;
        }

        int pct = static_cast<int>(((xp - currentLevelXp) * 100.0) / xpNeeded);
        int xpInLevel = std::clamp(static_cast<int>(xp - currentLevelXp), 0, static_cast<int>(xpNeeded));
        return LevelInfo{ std::clamp(pct, 0, 100), static_cast<int>(level), xpInLevel, static_cast<int>(xpNeeded) };
    }

    std::vector<uint16_t> ReadListUnits(uint32_t listAddr) const
    {
        std::vector<uint16_t> unitIds;
        uint32_t sentinel = ReadPtr(listAddr + 4);
        int32_t size = ReadInt(listAddr + 8);
        if (size <= 0 || size > 1000 || !IsValidPtr(sentinel))
        {
            return unitIds;
        }

        uint32_t current = ReadPtr(sentinel);
        for (int32_t i = 0; i < size; ++i)
        {
            if (current == 0 || current == sentinel || !IsValidPtr(current))
            {
                break;
            }

            uint16_t uid = 0;
            if (ReadBytes(current + 8, &uid, sizeof(uid)))
            {
                unitIds.push_back(uid);
            }
            current = ReadPtr(current);
        }

        return unitIds;
    }

    std::string GetStdString(uint32_t addr) const
    {
        if (!IsValidPtr(addr))
        {
            return "";
        }

        int32_t capacity = ReadInt(addr + 20);
        int32_t size = ReadInt(addr + 16);
        if (capacity < 0 || size < 0 || size > 1000)
        {
            return "";
        }

        std::vector<char> bytes(static_cast<size_t>(size));
        if (capacity < 16)
        {
            std::array<char, 16> inlineBytes = {};
            if (ReadBytes(addr, inlineBytes.data(), inlineBytes.size()))
            {
                std::copy(inlineBytes.begin(), inlineBytes.begin() + size, bytes.begin());
                return std::string(bytes.begin(), bytes.end());
            }
        }
        else
        {
            uint32_t ptr = ReadPtr(addr);
            if (ptr != 0 && ReadBytes(ptr, bytes.data(), bytes.size()))
            {
                return std::string(bytes.begin(), bytes.end());
            }
        }

        return "";
    }

    bool InheritsFromHero(uint32_t templatePtr) const
    {
        uint32_t current = templatePtr;
        for (int i = 0; i < 8; ++i)
        {
            if (!IsValidPtr(current))
            {
                break;
            }

            int32_t capacity = ReadInt(current + 28);
            int32_t size = ReadInt(current + 24);
            if (size > 0 && size < 100)
            {
                std::vector<char> bytes(static_cast<size_t>(size));
                bool ok = false;
                if (capacity < 16)
                {
                    std::array<char, 16> inlineBytes = {};
                    ok = ReadBytes(current + 8, inlineBytes.data(), inlineBytes.size());
                    if (ok)
                    {
                        std::copy(inlineBytes.begin(), inlineBytes.begin() + size, bytes.begin());
                    }
                }
                else
                {
                    uint32_t ptr = ReadPtr(current + 8);
                    ok = ReadBytes(ptr, bytes.data(), bytes.size());
                }

                if (ok)
                {
                    std::string name = LowerCopy(std::string(bytes.begin(), bytes.end()));
                    if (name.find("hero") != std::string::npos)
                    {
                        return true;
                    }
                }
            }

            current = ReadPtr(current + 0xe4);
        }

        return false;
    }

    HANDLE process_ = nullptr;
    LogFn logFn_ = nullptr;
    std::vector<int32_t> expTable_;
};

class OverlayApp
{
public:
    explicit OverlayApp(HINSTANCE instance) : instance_(instance)
    {
    }

    bool Init(int nCmdShow)
    {
        instanceForLog_ = this;
        BuildPreferencesPath();
        LoadPreferences();

        WNDCLASSEXW mainClass = {};
        mainClass.cbSize = sizeof(mainClass);
        mainClass.style = CS_HREDRAW | CS_VREDRAW;
        mainClass.lpfnWndProc = MainWndProc;
        mainClass.hInstance = instance_;
        mainClass.hIcon = LoadIcon(instance_, MAKEINTRESOURCE(IDI_HEROOVERLAYC));
        mainClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        mainClass.hbrBackground = CreateSolidBrush(kControlBg);
        mainClass.lpszClassName = L"ImperivmHeroOverlayControl";
        mainClass.hIconSm = LoadIcon(instance_, MAKEINTRESOURCE(IDI_SMALL));

        WNDCLASSEXW overlayClass = {};
        overlayClass.cbSize = sizeof(overlayClass);
        overlayClass.style = CS_HREDRAW | CS_VREDRAW;
        overlayClass.lpfnWndProc = OverlayWndProc;
        overlayClass.hInstance = instance_;
        overlayClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        overlayClass.hbrBackground = nullptr;
        overlayClass.lpszClassName = L"ImperivmHeroOverlayTransparent";

        if (!RegisterClassExW(&mainClass) || !RegisterClassExW(&overlayClass))
        {
            return false;
        }

        mainWnd_ = CreateWindowW(
            mainClass.lpszClassName,
            L"Imperivm Overlay Controller",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            0,
            540,
            315,
            nullptr,
            nullptr,
            instance_,
            this);

        if (!mainWnd_)
        {
            return false;
        }

        overlayWnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            overlayClass.lpszClassName,
            L"Imperivm Hero Overlay",
            WS_POPUP,
            0,
            0,
            kCanvasWidth,
            kCanvasHeight,
            nullptr,
            nullptr,
            instance_,
            this);

        if (!overlayWnd_)
        {
            return false;
        }

        SetLayeredWindowAttributes(overlayWnd_, kTransparentColor, 0, LWA_COLORKEY);
        ShowWindow(mainWnd_, nCmdShow);
        UpdateWindow(mainWnd_);
        ShowWindow(overlayWnd_, SW_SHOWNOACTIVATE);
        SetTimer(mainWnd_, kTimerId, kTimerMs, nullptr);

        Log("Overlay initialized. Searching for Imperivm window...");
        return true;
    }

    void BuildPreferencesPath()
    {
        wchar_t modulePath[MAX_PATH * 4] = {};
        DWORD size = GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
        if (size == 0 || size >= std::size(modulePath))
        {
            preferencesPath_ = L".\\hero_overlay_prefs.ini";
            return;
        }

        preferencesPath_ = modulePath;
        size_t slash = preferencesPath_.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            preferencesPath_ = L"hero_overlay_prefs.ini";
            return;
        }

        preferencesPath_ = preferencesPath_.substr(0, slash + 1) + L"hero_overlay_prefs.ini";
    }

    void LoadPreferences()
    {
        if (preferencesPath_.empty())
        {
            return;
        }

        offsetX_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"OffsetX", offsetX_, preferencesPath_.c_str())), 0, 1000);
        offsetY_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"OffsetY", offsetY_, preferencesPath_.c_str())), 0, 1000);
        gapY_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"GapY", gapY_, preferencesPath_.c_str())), 0, 200);
        int savedExperienceMode = static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"ExperienceMode", 0, preferencesPath_.c_str()));
        if (savedExperienceMode == 3)
        {
            expMode_ = ExperienceMode::HorizontalBars;
        }
        else if (savedExperienceMode == 2)
        {
            expMode_ = ExperienceMode::HorizontalReduced;
        }
        else if (savedExperienceMode == 1)
        {
            expMode_ = ExperienceMode::HorizontalBlocks;
        }
        else
        {
            expMode_ = ExperienceMode::Vertical;
        }
        numberStyle_ = GetPrivateProfileIntW(L"Overlay", L"NumberStyle", 0, preferencesPath_.c_str()) == 1 ? NumberStyle::Roman : NumberStyle::Arabic;
        levelColorsEnabled_ = GetPrivateProfileIntW(L"Overlay", L"LevelColors", 0, preferencesPath_.c_str()) != 0;
        int savedSkillPointsStyle = static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"SkillPointsStyle", 0, preferencesPath_.c_str()));
        skillPointsStyle_ = savedSkillPointsStyle == 1 ? SkillPointsStyle::ShowIndicator : SkillPointsStyle::ShowAll;

        wchar_t colorText[32] = {};
        GetPrivateProfileStringW(L"Colors", L"LevelBelow6", ColorToHex(levelColorLow_).c_str(), colorText, static_cast<DWORD>(std::size(colorText)), preferencesPath_.c_str());
        TryParseHexColor(colorText, levelColorLow_);
        GetPrivateProfileStringW(L"Colors", L"Level6", ColorToHex(levelColorWhite_).c_str(), colorText, static_cast<DWORD>(std::size(colorText)), preferencesPath_.c_str());
        TryParseHexColor(colorText, levelColorWhite_);
        GetPrivateProfileStringW(L"Colors", L"Level12", ColorToHex(levelColorBrown_).c_str(), colorText, static_cast<DWORD>(std::size(colorText)), preferencesPath_.c_str());
        TryParseHexColor(colorText, levelColorBrown_);
        GetPrivateProfileStringW(L"Colors", L"Level24", ColorToHex(levelColorGold_).c_str(), colorText, static_cast<DWORD>(std::size(colorText)), preferencesPath_.c_str());
        TryParseHexColor(colorText, levelColorGold_);
    }

    void SavePreferences() const
    {
        if (preferencesPath_.empty())
        {
            return;
        }

        WritePrivateProfileStringW(L"Overlay", L"OffsetX", IntToWide(offsetX_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"OffsetY", IntToWide(offsetY_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"GapY", IntToWide(gapY_).c_str(), preferencesPath_.c_str());
        const wchar_t* experienceModeValue = L"0";
        if (expMode_ == ExperienceMode::HorizontalBlocks)
        {
            experienceModeValue = L"1";
        }
        else if (expMode_ == ExperienceMode::HorizontalReduced)
        {
            experienceModeValue = L"2";
        }
        else if (expMode_ == ExperienceMode::HorizontalBars)
        {
            experienceModeValue = L"3";
        }
        WritePrivateProfileStringW(L"Overlay", L"ExperienceMode", experienceModeValue, preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"NumberStyle", numberStyle_ == NumberStyle::Roman ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"LevelColors", levelColorsEnabled_ ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"SkillPointsStyle", skillPointsStyle_ == SkillPointsStyle::ShowIndicator ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Colors", L"LevelBelow6", ColorToHex(levelColorLow_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Colors", L"Level6", ColorToHex(levelColorWhite_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Colors", L"Level12", ColorToHex(levelColorBrown_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Colors", L"Level24", ColorToHex(levelColorGold_).c_str(), preferencesPath_.c_str());
    }

    int Run()
    {
        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

private:
    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        OverlayApp* app = reinterpret_cast<OverlayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<OverlayApp*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }

        return app ? app->HandleMainMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        OverlayApp* app = reinterpret_cast<OverlayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<OverlayApp*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }

        return app ? app->HandleOverlayMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static void StaticLog(const std::string& message)
    {
        if (instanceForLog_)
        {
            instanceForLog_->Log(message);
        }
    }

    LRESULT HandleMainMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            CreateControls(hwnd);
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
            return HandleControlColor(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

        case WM_COMMAND:
            HandleCommand(LOWORD(wParam), HIWORD(wParam));
            return 0;

        case WM_TIMER:
            if (wParam == kTimerId)
            {
                Tick();
            }
            return 0;

        case WM_SIZE:
            LayoutControls();
            return 0;

        case WM_DESTROY:
            Shutdown();
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT HandleOverlayMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            PaintOverlay(hwnd);
            return 0;

        case WM_NCHITTEST:
            return testMode_ ? HTCLIENT : HTTRANSPARENT;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void CreateControls(HWND parent)
    {
        HFONT guiFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        titleFont_ = CreateFontW(
            28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Trebuchet MS");

        authorFont_ = CreateFontW(
            13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Calibri");

        titleLabel_ = CreateWindowW(L"STATIC", L"XPview", WS_CHILD | WS_VISIBLE, 15, 12, 150, 32, parent, nullptr, instance_, nullptr);
        authorLabel_ = CreateWindowW(L"STATIC", L"Created by Upercat", WS_CHILD | WS_VISIBLE, 15, 42, 200, 18, parent, nullptr, instance_, nullptr);

        SendMessageW(titleLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        SendMessageW(authorLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(authorFont_), TRUE);

        std::wstring offsetXText = IntToWide(offsetX_);
        std::wstring offsetYText = IntToWide(offsetY_);
        std::wstring gapYText = IntToWide(gapY_);

        offsetXLabel_ = CreateWindowW(L"STATIC", L"Offset X:", WS_CHILD | WS_VISIBLE, 10, 70, 70, 22, parent, nullptr, instance_, nullptr);
        offsetXEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", offsetXText.c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 80, 66, 55, 24, parent, reinterpret_cast<HMENU>(IDC_OFFSET_X), instance_, nullptr);

        offsetYLabel_ = CreateWindowW(L"STATIC", L"Offset Y:", WS_CHILD | WS_VISIBLE, 150, 70, 70, 22, parent, nullptr, instance_, nullptr);
        offsetYEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", offsetYText.c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 220, 66, 55, 24, parent, reinterpret_cast<HMENU>(IDC_OFFSET_Y), instance_, nullptr);

        gapYLabel_ = CreateWindowW(L"STATIC", L"Gap Y:", WS_CHILD | WS_VISIBLE, 290, 70, 55, 22, parent, nullptr, instance_, nullptr);
        gapYEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", gapYText.c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 345, 66, 55, 24, parent, reinterpret_cast<HMENU>(IDC_GAP_Y), instance_, nullptr);

        expModeLabel_ = CreateWindowW(L"STATIC", L"XP Mode:", WS_CHILD | WS_VISIBLE, 10, 108, 70, 22, parent, nullptr, instance_, nullptr);
        expModeCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 80, 104, 180, 200, parent, reinterpret_cast<HMENU>(IDC_EXP_MODE), instance_, nullptr);
        SendMessageW(expModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Vertical"));
        SendMessageW(expModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Horizontal blocks"));
        SendMessageW(expModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Horizontal reduced"));
        SendMessageW(expModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Horizontal bars"));
        int expModeSelection = 0;
        if (expMode_ == ExperienceMode::HorizontalBlocks)
        {
            expModeSelection = 1;
        }
        else if (expMode_ == ExperienceMode::HorizontalReduced)
        {
            expModeSelection = 2;
        }
        else if (expMode_ == ExperienceMode::HorizontalBars)
        {
            expModeSelection = 3;
        }
        SendMessageW(expModeCombo_, CB_SETCURSEL, expModeSelection, 0);

        numberStyleLabel_ = CreateWindowW(L"STATIC", L"Numbers:", WS_CHILD | WS_VISIBLE, 290, 108, 70, 22, parent, nullptr, instance_, nullptr);
        numberStyleCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 360, 104, 150, 200, parent, reinterpret_cast<HMENU>(IDC_NUMBER_STYLE), instance_, nullptr);
        SendMessageW(numberStyleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Arabic"));
        SendMessageW(numberStyleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Roman"));
        SendMessageW(numberStyleCombo_, CB_SETCURSEL, numberStyle_ == NumberStyle::Roman ? 1 : 0, 0);

        levelColorsCheck_ = CreateWindowW(L"BUTTON", L"Game level colors", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 146, 200, 24, parent, reinterpret_cast<HMENU>(IDC_LEVEL_COLORS), instance_, nullptr);
        SendMessageW(levelColorsCheck_, BM_SETCHECK, levelColorsEnabled_ ? BST_CHECKED : BST_UNCHECKED, 0);

        skillPointsLabel_ = CreateWindowW(L"STATIC", L"Skill points:", WS_CHILD | WS_VISIBLE, 290, 146, 90, 22, parent, nullptr, instance_, nullptr);
        skillPointsCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 380, 142, 130, 200, parent, reinterpret_cast<HMENU>(IDC_SKILL_POINTS), instance_, nullptr);
        SendMessageW(skillPointsCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Show all"));
        SendMessageW(skillPointsCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Show indicator"));
        SendMessageW(skillPointsCombo_, CB_SETCURSEL, skillPointsStyle_ == SkillPointsStyle::ShowIndicator ? 1 : 0, 0);

        colorLowLabel_ = CreateWindowW(L"STATIC", L"<6:", WS_CHILD | WS_VISIBLE, 10, 180, 30, 22, parent, nullptr, instance_, nullptr);
        colorLowEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorLow_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 42, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_LOW), instance_, nullptr);

        colorWhiteLabel_ = CreateWindowW(L"STATIC", L"6+:", WS_CHILD | WS_VISIBLE, 128, 180, 30, 22, parent, nullptr, instance_, nullptr);
        colorWhiteEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorWhite_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 160, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_WHITE), instance_, nullptr);

        colorBrownLabel_ = CreateWindowW(L"STATIC", L"12+:", WS_CHILD | WS_VISIBLE, 246, 180, 38, 22, parent, nullptr, instance_, nullptr);
        colorBrownEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorBrown_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 288, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_BROWN), instance_, nullptr);

        colorGoldLabel_ = CreateWindowW(L"STATIC", L"24+:", WS_CHILD | WS_VISIBLE, 374, 180, 38, 22, parent, nullptr, instance_, nullptr);
        colorGoldEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorGold_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 416, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_GOLD), instance_, nullptr);

        applyButton_ = CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 415, 64, 105, 28, parent, reinterpret_cast<HMENU>(IDC_APPLY), instance_, nullptr);
        testButton_ = CreateWindowW(L"BUTTON", L"TEST OVERLAY", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 220, 140, 32, parent, reinterpret_cast<HMENU>(IDC_TEST), instance_, nullptr);
        exitButton_ = CreateWindowW(L"BUTTON", L"EXIT OVERLAY", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 380, 220, 140, 32, parent, reinterpret_cast<HMENU>(IDC_EXIT_OVERLAY), instance_, nullptr);

        std::array<HWND, 24> controls = { offsetXLabel_, offsetXEdit_, offsetYLabel_, offsetYEdit_, gapYLabel_, gapYEdit_, expModeLabel_, expModeCombo_, numberStyleLabel_, numberStyleCombo_, levelColorsCheck_, skillPointsLabel_, skillPointsCombo_, colorLowLabel_, colorLowEdit_, colorWhiteLabel_, colorWhiteEdit_, colorBrownLabel_, colorBrownEdit_, colorGoldLabel_, colorGoldEdit_, applyButton_, testButton_, exitButton_ };
        for (HWND control : controls)
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(guiFont), TRUE);
        }

        LayoutControls();
    }

    LRESULT HandleControlColor(HDC hdc, HWND control)
    {
        SetBkMode(hdc, OPAQUE);
        if (control == titleLabel_)
        {
            SetTextColor(hdc, RGB(249, 226, 175));
            SetBkColor(hdc, kControlBg);
            return reinterpret_cast<LRESULT>(controlBrush_);
        }
        if (control == authorLabel_)
        {
            SetTextColor(hdc, RGB(166, 173, 200));
            SetBkColor(hdc, kControlBg);
            return reinterpret_cast<LRESULT>(controlBrush_);
        }

        SetTextColor(hdc, RGB(205, 214, 244));
        SetBkColor(hdc, kControlBg);
        return reinterpret_cast<LRESULT>(controlBrush_);
    }

    void LayoutControls()
    {
        if (!mainWnd_)
        {
            return;
        }

        RECT client = {};
        GetClientRect(mainWnd_, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;

        MoveWindow(titleLabel_, 15, 12, 150, 32, TRUE);
        MoveWindow(authorLabel_, 15, 42, 200, 18, TRUE);

        int rowY = 70;
        MoveWindow(offsetXLabel_, 10, rowY, 70, 22, TRUE);
        MoveWindow(offsetYLabel_, 150, rowY, 70, 22, TRUE);
        MoveWindow(gapYLabel_, 290, rowY, 55, 22, TRUE);
        MoveWindow(offsetXEdit_, 80, rowY - 4, 55, 24, TRUE);
        MoveWindow(offsetYEdit_, 220, rowY - 4, 55, 24, TRUE);
        MoveWindow(gapYEdit_, 345, rowY - 4, 55, 24, TRUE);
        MoveWindow(applyButton_, max(415, width - 125), rowY - 6, 105, 28, TRUE);

        int modeY = rowY + 38;
        MoveWindow(expModeLabel_, 10, modeY, 70, 22, TRUE);
        MoveWindow(expModeCombo_, 80, modeY - 4, 180, 200, TRUE);
        MoveWindow(numberStyleLabel_, 290, modeY, 70, 22, TRUE);
        MoveWindow(numberStyleCombo_, 360, modeY - 4, 150, 200, TRUE);

        int colorsY = rowY + 76;
        MoveWindow(levelColorsCheck_, 10, colorsY, 210, 24, TRUE);
        MoveWindow(skillPointsLabel_, 290, colorsY, 90, 22, TRUE);
        MoveWindow(skillPointsCombo_, 380, colorsY - 4, 130, 200, TRUE);

        int paletteY = rowY + 110;
        MoveWindow(colorLowLabel_, 10, paletteY, 30, 22, TRUE);
        MoveWindow(colorLowEdit_, 42, paletteY - 4, 76, 24, TRUE);
        MoveWindow(colorWhiteLabel_, 128, paletteY, 30, 22, TRUE);
        MoveWindow(colorWhiteEdit_, 160, paletteY - 4, 76, 24, TRUE);
        MoveWindow(colorBrownLabel_, 246, paletteY, 38, 22, TRUE);
        MoveWindow(colorBrownEdit_, 288, paletteY - 4, 76, 24, TRUE);
        MoveWindow(colorGoldLabel_, 374, paletteY, 38, 22, TRUE);
        MoveWindow(colorGoldEdit_, 416, paletteY - 4, 76, 24, TRUE);

        int buttonY = rowY + 150;
        MoveWindow(testButton_, 10, buttonY, 140, 32, TRUE);
        MoveWindow(exitButton_, max(160, width - 150), buttonY, 140, 32, TRUE);
    }

    void HandleCommand(int id, int notifyCode)
    {
        switch (id)
        {
        case IDC_APPLY:
            ApplyOffsetsFromControls();
            break;

        case IDC_TEST:
            ToggleTestMode();
            break;

        case IDC_EXIT_OVERLAY:
        case IDM_EXIT:
            DestroyWindow(mainWnd_);
            break;

        case IDC_EXP_MODE:
            if (notifyCode == CBN_SELCHANGE)
            {
                ApplyExperienceModeFromControl();
            }
            break;

        case IDC_NUMBER_STYLE:
            if (notifyCode == CBN_SELCHANGE)
            {
                ApplyNumberStyleFromControl();
            }
            break;

        case IDC_LEVEL_COLORS:
            if (notifyCode == BN_CLICKED)
            {
                ApplyLevelColorsFromControl();
            }
            break;

        case IDC_SKILL_POINTS:
            if (notifyCode == CBN_SELCHANGE)
            {
                ApplySkillPointsFromControl();
            }
            break;

        case IDM_ABOUT:
            MessageBoxW(mainWnd_, L"Imperivm Hero Overlay C++ port", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        }
    }

    int ReadIntEdit(HWND edit, int fallback) const
    {
        wchar_t text[32] = {};
        GetWindowTextW(edit, text, static_cast<int>(std::size(text)));
        wchar_t* end = nullptr;
        long value = wcstol(text, &end, 10);
        if (end == text)
        {
            return fallback;
        }
        return static_cast<int>(value);
    }

    COLORREF ReadColorEdit(HWND edit, COLORREF fallback, const char* label)
    {
        wchar_t text[32] = {};
        GetWindowTextW(edit, text, static_cast<int>(std::size(text)));
        COLORREF parsed = fallback;
        if (!TryParseHexColor(text, parsed))
        {
            SetWindowTextW(edit, ColorToHex(fallback).c_str());
            Log(std::string("Invalid color for ") + label + "; keeping " + ToNarrowLossy(ColorToHex(fallback)));
            return fallback;
        }

        SetWindowTextW(edit, ColorToHex(parsed).c_str());
        return parsed;
    }

    void ApplyOffsetsFromControls()
    {
        offsetX_ = std::clamp(ReadIntEdit(offsetXEdit_, offsetX_), 0, 1000);
        offsetY_ = std::clamp(ReadIntEdit(offsetYEdit_, offsetY_), 0, 1000);
        gapY_ = std::clamp(ReadIntEdit(gapYEdit_, gapY_), 0, 200);
        levelColorLow_ = ReadColorEdit(colorLowEdit_, levelColorLow_, "<6");
        levelColorWhite_ = ReadColorEdit(colorWhiteEdit_, levelColorWhite_, "6+");
        levelColorBrown_ = ReadColorEdit(colorBrownEdit_, levelColorBrown_, "12+");
        levelColorGold_ = ReadColorEdit(colorGoldEdit_, levelColorGold_, "24+");

        std::ostringstream msg;
        msg << "Settings updated - X: " << offsetX_ << ", Y: " << offsetY_ << ", Gap: " << gapY_;
        Log(msg.str());
        SavePreferences();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ApplyExperienceModeFromControl()
    {
        int selected = static_cast<int>(SendMessageW(expModeCombo_, CB_GETCURSEL, 0, 0));
        if (selected == 3)
        {
            expMode_ = ExperienceMode::HorizontalBars;
        }
        else if (selected == 2)
        {
            expMode_ = ExperienceMode::HorizontalReduced;
        }
        else if (selected == 1)
        {
            expMode_ = ExperienceMode::HorizontalBlocks;
        }
        else
        {
            expMode_ = ExperienceMode::Vertical;
        }

        std::string modeName = "Vertical";
        if (expMode_ == ExperienceMode::HorizontalBlocks)
        {
            modeName = "Horizontal blocks";
        }
        else if (expMode_ == ExperienceMode::HorizontalReduced)
        {
            modeName = "Horizontal reduced";
        }
        else if (expMode_ == ExperienceMode::HorizontalBars)
        {
            modeName = "Horizontal bars";
        }
        Log("Experience mode: " + modeName);
        SavePreferences();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ApplyNumberStyleFromControl()
    {
        int selected = static_cast<int>(SendMessageW(numberStyleCombo_, CB_GETCURSEL, 0, 0));
        numberStyle_ = selected == 1 ? NumberStyle::Roman : NumberStyle::Arabic;
        Log(std::string("Number style: ") + (numberStyle_ == NumberStyle::Roman ? "Roman" : "Arabic"));
        SavePreferences();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ApplyLevelColorsFromControl()
    {
        levelColorsEnabled_ = SendMessageW(levelColorsCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        Log(std::string("Level colors: ") + (levelColorsEnabled_ ? "enabled" : "disabled"));
        SavePreferences();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ApplySkillPointsFromControl()
    {
        int selected = static_cast<int>(SendMessageW(skillPointsCombo_, CB_GETCURSEL, 0, 0));
        skillPointsStyle_ = selected == 1 ? SkillPointsStyle::ShowIndicator : SkillPointsStyle::ShowAll;
        Log(std::string("Skill points style: ") + (skillPointsStyle_ == SkillPointsStyle::ShowIndicator ? "Show indicator" : "Show all"));
        SavePreferences();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ToggleTestMode()
    {
        testMode_ = !testMode_;

        LONG_PTR exStyle = GetWindowLongPtrW(overlayWnd_, GWL_EXSTYLE);
        if (testMode_)
        {
            exStyle &= ~WS_EX_TRANSPARENT;
            SetWindowLongPtrW(overlayWnd_, GWL_EXSTYLE, exStyle);
            SetLayeredWindowAttributes(overlayWnd_, 0, 255, LWA_ALPHA);
            SetWindowTextW(testButton_, L"SET TRANSPARENT");
            Log("Test Mode Activated: solid background and guide border should be visible.");
        }
        else
        {
            exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
            SetWindowLongPtrW(overlayWnd_, GWL_EXSTYLE, exStyle);
            SetLayeredWindowAttributes(overlayWnd_, kTransparentColor, 0, LWA_COLORKEY);
            SetWindowTextW(testButton_, L"TEST OVERLAY");
            Log("Transparency restored.");
        }

        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void UpdateLevelFlashState(const std::array<SlotInfo, 5>& newSlots)
    {
        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < newSlots.size(); ++i)
        {
            const SlotInfo& previous = previousSlots_[i];
            const SlotInfo& current = newSlots[i];
            bool previousVisible = previous.pct >= 0;
            bool currentVisible = current.pct >= 0;
            bool previousComplete = previousVisible && previous.xpNeeded > 0 && previous.xpInLevel >= previous.xpNeeded;
            bool currentComplete = currentVisible && current.xpNeeded > 0 && current.xpInLevel >= current.xpNeeded;
            bool levelAdvanced = previousVisible && currentVisible && current.level > previous.level;

            if ((currentComplete && !previousComplete) || levelAdvanced)
            {
                int sourceLevel = levelAdvanced ? previous.level : current.level;
                int sourceXpNeeded = levelAdvanced ? previous.xpNeeded : current.xpNeeded;
                flashUntil_[i] = now + std::chrono::milliseconds(kLevelFlashMs);
                flashStart_[i] = now;
                flashLevel_[i] = sourceLevel + 1;
                flashSegments_[i] = max(1, sourceXpNeeded);
            }
        }

        previousSlots_ = newSlots;
    }

    void Tick()
    {
        if (!process_.process || !IsWindow(process_.hwnd))
        {
            DisconnectProcess();
            auto now = std::chrono::steady_clock::now();
            if (now - lastSearchAttempt_ < std::chrono::seconds(1))
            {
                return;
            }
            lastSearchAttempt_ = now;

            if (now - lastWaitingLog_ >= std::chrono::seconds(10))
            {
                lastWaitingLog_ = now;
                Log("Waiting for gbr.exe or gbr_custom.exe...");
            }

            GameProcess found = FindGameProcess(StaticLog);
            if (!found.process || !found.hwnd)
            {
                return;
            }

            process_ = found;
            reader_ = std::make_unique<GameMemoryReader>(process_.process, StaticLog);
            lastWaitingLog_ = std::chrono::steady_clock::now();
            Log("Successfully connected to game window: '" + ToNarrowLossy(process_.title) + "'");
        }

        RECT rect = {};
        if (!GetWindowRect(process_.hwnd, &rect))
        {
            DisconnectProcess();
            return;
        }

        int x = rect.left;
        int y = rect.top;
        if (x < -32000 || y < -32000)
        {
            ShowWindow(overlayWnd_, SW_HIDE);
            return;
        }

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        if (x != lastX_ || y != lastY_ || width != lastWidth_ || height != lastHeight_)
        {
            lastX_ = x;
            lastY_ = y;
            lastWidth_ = width;
            lastHeight_ = height;
            SetWindowPos(overlayWnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
        }
        else
        {
            auto now = std::chrono::steady_clock::now();
            if (now - lastTopmostUpdate_ >= std::chrono::seconds(1))
            {
                lastTopmostUpdate_ = now;
                SetWindowPos(overlayWnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
        }

        uint32_t mapPtr = reader_ ? reader_->ReadPtr(0x008c8e60) : 0;
        TacticalMapState tacticalMap = reader_ ? reader_->ReadTacticalMapState() : TacticalMapState{};
        bool globalMapActive = mapPtr != 0;
        bool mapOverlayBlocked = globalMapActive || tacticalMap.open;
        if (mapOverlayBlocked && overlayVisible_)
        {
            overlayVisible_ = false;
            ShowWindow(overlayWnd_, SW_HIDE);
        }
        else if (!mapOverlayBlocked && !overlayVisible_)
        {
            overlayVisible_ = true;
            ShowWindow(overlayWnd_, SW_SHOWNOACTIVATE);
        }

        slots_ = mapOverlayBlocked || !reader_ ? std::array<SlotInfo, 5>() : reader_->ScanHeroSlots();
        UpdateLevelFlashState(slots_);

        InvalidateRect(overlayWnd_, nullptr, FALSE);

        auto now = std::chrono::steady_clock::now();
        if (kVerboseLogging && now - lastSlotLog_ >= std::chrono::seconds(2))
        {
            lastSlotLog_ = now;

            Log("Global Map ptr (0x008c8e60): " + Hex32(mapPtr) + " (active=" + (globalMapActive ? "true" : "false") + ")");
            Log("Tactical Map state: view_ctrl=" + Hex32(tacticalMap.viewCtrl) +
                ", flag=" + std::to_string(tacticalMap.flag) +
                " (open=" + (tacticalMap.open ? "true" : "false") + ")");

            std::ostringstream status;
            status << "Slots Status: ";
            for (size_t i = 0; i < slots_.size(); ++i)
            {
                if (i > 0)
                {
                    status << " | ";
                }
                status << "S" << (i + 1) << ": ";
                if (slots_[i].pct >= 0)
                {
                    status << slots_[i].label << " (" << slots_[i].pct << "%, skill+=" << slots_[i].availableSkillPoints << ")";
                }
                else
                {
                    status << "Empty";
                }
            }
            Log(status.str());

            std::ostringstream location;
            location << "Game window location: X=" << x << ", Y=" << y;
            Log(location.str());
        }
    }

    void PaintOverlay(HWND hwnd)
    {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!hdc)
        {
            return;
        }
        auto paintNow = std::chrono::steady_clock::now();

        RECT clientRect = {};
        GetClientRect(hwnd, &clientRect);
        int overlayWidth = clientRect.right - clientRect.left;
        int overlayHeight = clientRect.bottom - clientRect.top;

        HDC paintDc = hdc;
        HDC memoryDc = CreateCompatibleDC(hdc);
        HBITMAP memoryBitmap = memoryDc ? CreateCompatibleBitmap(hdc, overlayWidth, overlayHeight) : nullptr;
        HGDIOBJ oldMemoryBitmap = nullptr;
        if (memoryDc && memoryBitmap)
        {
            oldMemoryBitmap = SelectObject(memoryDc, memoryBitmap);
            if (!oldMemoryBitmap)
            {
                DeleteObject(memoryBitmap);
                DeleteDC(memoryDc);
                memoryDc = nullptr;
                memoryBitmap = nullptr;
            }
            else
            {
                paintDc = memoryDc;
            }
        }

        HBRUSH bgBrush = CreateSolidBrush(testMode_ ? RGB(45, 45, 45) : kTransparentColor);
        RECT canvas = { 0, 0, overlayWidth, overlayHeight };
        FillRect(paintDc, &canvas, bgBrush);
        DeleteObject(bgBrush);

        if (testMode_)
        {
            HBRUSH green = CreateSolidBrush(RGB(0, 255, 0));
            RECT guideFill = { 0, 0, overlayWidth, overlayHeight };
            FillRect(paintDc, &guideFill, green);
            DeleteObject(green);

            HPEN redPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HGDIOBJ oldPen = SelectObject(paintDc, redPen);
            HGDIOBJ oldBrush = SelectObject(paintDc, GetStockObject(NULL_BRUSH));
            Rectangle(paintDc, 1, 1, overlayWidth - 1, overlayHeight - 1);
            SelectObject(paintDc, oldBrush);
            SelectObject(paintDc, oldPen);
            DeleteObject(redPen);
        }

        HBRUSH barBgBrush = CreateSolidBrush(kBarBg);
        HBRUSH blockEmptyBrush = CreateSolidBrush(kBlockEmpty);

        for (size_t i = 0; i < slots_.size(); ++i)
        {
            int yStart = offsetY_ + static_cast<int>(i) * gapY_;
            bool visible = slots_[i].pct >= 0 || testMode_;
            if (!visible)
            {
                continue;
            }

            int displayPct = slots_[i].pct >= 0 ? slots_[i].pct : 50;
            int displayLevel = slots_[i].pct >= 0 ? slots_[i].level + 1 : 12;
            int displayXpInLevel = slots_[i].pct >= 0 ? slots_[i].xpInLevel : 5;
            int displayXpNeeded = slots_[i].pct >= 0 ? slots_[i].xpNeeded : 10;
            int displaySkillPoints = slots_[i].pct >= 0 ? slots_[i].availableSkillPoints : 2;
            bool flashing = paintNow < flashUntil_[i];
            bool flashBlinkOn = true;
            if (flashing)
            {
                auto flashElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(paintNow - flashStart_[i]).count();
                flashBlinkOn = ((flashElapsed / 100) % 2) == 0;
                displayPct = 100;
                displayLevel = flashLevel_[i];
                displayXpNeeded = flashSegments_[i];
                displayXpInLevel = flashSegments_[i];
            }

            bool horizontalMode = expMode_ != ExperienceMode::Vertical;
            bool reducedHorizontalMode = expMode_ == ExperienceMode::HorizontalReduced;
            COLORREF fillColor = GetExperienceFillColor(displayLevel, flashing, horizontalMode ? kBlockFilled : kBarFg);
            COLORREF outlineBase = levelColorsEnabled_ ? BrightenColor(GetLevelBaseColor(displayLevel), 35) : kBlockOutline;
            COLORREF outlineColor = flashing ? BrightenColor(outlineBase, 40) : outlineBase;
            HBRUSH fillBrush = CreateSolidBrush(fillColor);

            if (horizontalMode)
            {
                PaintHorizontalBlocks(paintDc, blockEmptyBrush, fillBrush, outlineColor, offsetX_, yStart, displayLevel, displaySkillPoints, displayXpInLevel, displayXpNeeded, !flashing || flashBlinkOn, expMode_);
                DeleteObject(fillBrush);
                continue;
            }

            PaintLevelText(paintDc, displayLevel, displaySkillPoints, offsetX_ + kIndicatorWidth + kVerticalLevelPaddingX, yStart, 90, DT_LEFT);

            RECT bg = { offsetX_, yStart, offsetX_ + kIndicatorWidth, yStart + kIndicatorHeight };
            FillRect(paintDc, &bg, barBgBrush);

            int barHeight = static_cast<int>((displayPct / 100.0) * kIndicatorHeight);
            RECT fg = {
                offsetX_,
                yStart + kIndicatorHeight - barHeight,
                offsetX_ + kIndicatorWidth,
                yStart + kIndicatorHeight
            };
            if (!flashing || flashBlinkOn)
            {
                FillRect(paintDc, &fg, fillBrush);
            }
            DeleteObject(fillBrush);
        }

        DeleteObject(blockEmptyBrush);
        DeleteObject(barBgBrush);
        if (paintDc == memoryDc && memoryDc)
        {
            BitBlt(hdc, 0, 0, overlayWidth, overlayHeight, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldMemoryBitmap);
            DeleteObject(memoryBitmap);
            DeleteDC(memoryDc);
        }
        else if (memoryDc)
        {
            DeleteDC(memoryDc);
        }
        EndPaint(hwnd, &ps);
    }

    COLORREF BrightenColor(COLORREF color, int amount) const
    {
        int red = min(255, GetRValue(color) + amount);
        int green = min(255, GetGValue(color) + amount);
        int blue = min(255, GetBValue(color) + amount);
        return RGB(red, green, blue);
    }

    COLORREF GetLevelBaseColor(int level) const
    {
        if (level >= 24)
        {
            return levelColorGold_;
        }
        if (level >= 12)
        {
            return levelColorBrown_;
        }
        if (level >= 6)
        {
            return levelColorWhite_;
        }
        return levelColorLow_;
    }

    COLORREF GetExperienceFillColor(int level, bool flashing, COLORREF fallback) const
    {
        COLORREF color = levelColorsEnabled_ ? GetLevelBaseColor(level) : fallback;
        return flashing ? BrightenColor(color, 70) : color;
    }

    std::wstring FormatLevelText(int level) const
    {
        if (numberStyle_ == NumberStyle::Arabic)
        {
            return std::to_wstring(level);
        }

        struct RomanPart
        {
            int value;
            const wchar_t* text;
        };

        static constexpr std::array<RomanPart, 13> parts = { {
            {1000, L"M"},
            {900, L"CM"},
            {500, L"D"},
            {400, L"CD"},
            {100, L"C"},
            {90, L"XC"},
            {50, L"L"},
            {40, L"XL"},
            {10, L"X"},
            {9, L"IX"},
            {5, L"V"},
            {4, L"IV"},
            {1, L"I"},
        } };

        int remaining = max(1, level);
        std::wstring out;
        for (const RomanPart& part : parts)
        {
            while (remaining >= part.value)
            {
                out += part.text;
                remaining -= part.value;
            }
        }
        return out;
    }

    void PaintLevelText(HDC hdc, int level, int availableSkillPoints, int x, int y, int width, UINT align) const
    {
        std::wstring levelText = FormatLevelText(level);
        if (availableSkillPoints > 0)
        {
            if (skillPointsStyle_ == SkillPointsStyle::ShowIndicator)
            {
                levelText += L"+";
            }
            else
            {
                levelText += std::wstring(static_cast<size_t>(availableSkillPoints), L'+');
            }
        }
        RECT levelRect = { x, y, x + width, y + kLevelTextHeight };
        HFONT font = CreateFontW(
            kLevelFontHeight,
            0,
            0,
            0,
            FW_BOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Consolas");

        HGDIOBJ oldFont = font ? SelectObject(hdc, font) : nullptr;
        int oldBkMode = SetBkMode(hdc, TRANSPARENT);
        COLORREF oldTextColor = SetTextColor(hdc, kLevelTextColor);
        DrawTextW(hdc, levelText.c_str(), -1, &levelRect, align | DT_SINGLELINE | DT_VCENTER | DT_NOCLIP);
        SetTextColor(hdc, oldTextColor);
        SetBkMode(hdc, oldBkMode);
        if (oldFont)
        {
            SelectObject(hdc, oldFont);
        }
        if (font)
        {
            DeleteObject(font);
        }
    }

    void PaintHorizontalBlocks(HDC hdc, HBRUSH emptyBrush, HBRUSH filledBrush, COLORREF outlineColor, int x, int yStart, int level, int availableSkillPoints, int xpInLevel, int xpNeeded, bool showFilledBlocks, ExperienceMode expMode)
    {
        int blockX = x + kHorizontalPaddingX;
        int y = yStart + kHorizontalYOffset;
        int segments = max(1, xpNeeded);
        int filledSegments = std::clamp(xpInLevel, 0, segments);
        int gaps = segments - 1;
        bool reducedMode = expMode == ExperienceMode::HorizontalReduced;
        bool barsMode = expMode == ExperienceMode::HorizontalBars;
        int baseWidth = (reducedMode) ? kHorizontalReducedBlockBaseWidth : kHorizontalBlockBaseWidth;
        int widthLoss = (reducedMode) ? max(0, (level - 10) / 2) : max(0, (level - 10) / 4);
        int segmentWidth = std::clamp(baseWidth - widthLoss, kHorizontalBlockMinWidth, baseWidth);
        int horizontalGap = level > 40 ? 2 : kHorizontalGap;
        int usedWidth = segments * segmentWidth + gaps * horizontalGap;

        PaintLevelText(hdc, level, availableSkillPoints, blockX, yStart + kLevelTextYOffset, max(usedWidth, 90), DT_LEFT);

        HPEN outlinePen = CreatePen(PS_SOLID, 1, outlineColor);
        HGDIOBJ oldPen = SelectObject(hdc, outlinePen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        bool continuousBar = barsMode || (reducedMode && level >= 50);

        if (continuousBar)
        {
            RECT block = {
                blockX,
                y,
                blockX + usedWidth,
                y + kHorizontalHeight
            };
            FillRect(hdc, &block, emptyBrush);
            int filledWidth = (filledSegments * usedWidth) / segments;
            if (showFilledBlocks && filledWidth > 0)
            {
                RECT filledBlock = {
                    blockX,
                    y,
                    blockX + filledWidth,
                    y + kHorizontalHeight
                };
                FillRect(hdc, &filledBlock, filledBrush);
            }
            Rectangle(hdc, block.left, block.top, block.right, block.bottom);
        }
        else
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                int segmentX = blockX + segment * (segmentWidth + horizontalGap);
                RECT block = {
                    segmentX,
                    y,
                    segmentX + segmentWidth,
                    y + kHorizontalHeight
                };
                FillRect(hdc, &block, showFilledBlocks && segment < filledSegments ? filledBrush : emptyBrush);
                Rectangle(hdc, block.left, block.top, block.right, block.bottom);
            }
        }

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(outlinePen);
    }

    void Log(const std::string& message)
    {
        if constexpr (kVerboseLogging)
        {
            SYSTEMTIME now = {};
            GetLocalTime(&now);

            char timestamp[32] = {};
            std::snprintf(timestamp, sizeof(timestamp), "[%02u:%02u:%02u] ", now.wHour, now.wMinute, now.wSecond);

            std::wstring line = ToWide(std::string(timestamp) + message + "\r\n");
            OutputDebugStringW(line.c_str());
        }
    }

    void DisconnectProcess()
    {
        if (process_.process)
        {
            CloseHandle(process_.process);
            Log("Game process disconnected.");
        }
        process_ = {};
        reader_.reset();
    }

    void Shutdown()
    {
        SavePreferences();
        KillTimer(mainWnd_, kTimerId);
        DisconnectProcess();
        if (overlayWnd_)
        {
            DestroyWindow(overlayWnd_);
            overlayWnd_ = nullptr;
        }
        if (titleFont_)
        {
            DeleteObject(titleFont_);
            titleFont_ = nullptr;
        }
        if (authorFont_)
        {
            DeleteObject(authorFont_);
            authorFont_ = nullptr;
        }
    }

    HINSTANCE instance_ = nullptr;
    HWND mainWnd_ = nullptr;
    HWND overlayWnd_ = nullptr;
    HWND titleLabel_ = nullptr;
    HWND authorLabel_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT authorFont_ = nullptr;
    HWND skillPointsLabel_ = nullptr;
    HWND skillPointsCombo_ = nullptr;
    HWND offsetXLabel_ = nullptr;
    HWND offsetXEdit_ = nullptr;
    HWND offsetYLabel_ = nullptr;
    HWND offsetYEdit_ = nullptr;
    HWND gapYLabel_ = nullptr;
    HWND gapYEdit_ = nullptr;
    HWND expModeLabel_ = nullptr;
    HWND expModeCombo_ = nullptr;
    HWND numberStyleLabel_ = nullptr;
    HWND numberStyleCombo_ = nullptr;
    HWND levelColorsCheck_ = nullptr;
    HWND colorLowLabel_ = nullptr;
    HWND colorLowEdit_ = nullptr;
    HWND colorWhiteLabel_ = nullptr;
    HWND colorWhiteEdit_ = nullptr;
    HWND colorBrownLabel_ = nullptr;
    HWND colorBrownEdit_ = nullptr;
    HWND colorGoldLabel_ = nullptr;
    HWND colorGoldEdit_ = nullptr;
    HWND applyButton_ = nullptr;
    HWND testButton_ = nullptr;
    HWND exitButton_ = nullptr;
    HBRUSH controlBrush_ = CreateSolidBrush(kControlBg);
    GameProcess process_;
    std::unique_ptr<GameMemoryReader> reader_;
    std::array<SlotInfo, 5> slots_;
    std::array<SlotInfo, 5> previousSlots_;
    std::array<std::chrono::steady_clock::time_point, 5> flashStart_ = {};
    std::array<std::chrono::steady_clock::time_point, 5> flashUntil_ = {};
    std::array<int, 5> flashSegments_ = { 1, 1, 1, 1, 1 };
    std::array<int, 5> flashLevel_ = { 1, 1, 1, 1, 1 };
    std::wstring preferencesPath_;
    int offsetX_ = 60;
    int offsetY_ = 88;
    int gapY_ = 68;
    int lastX_ = -1;
    int lastY_ = -1;
    int lastWidth_ = -1;
    int lastHeight_ = -1;
    bool testMode_ = false;
    bool overlayVisible_ = true;
    ExperienceMode expMode_ = ExperienceMode::Vertical;
    NumberStyle numberStyle_ = NumberStyle::Arabic;
    SkillPointsStyle skillPointsStyle_ = SkillPointsStyle::ShowAll;
    bool levelColorsEnabled_ = false;
    COLORREF levelColorLow_ = RGB(146, 163, 204);
    COLORREF levelColorWhite_ = RGB(248, 250, 255);
    COLORREF levelColorBrown_ = RGB(185, 105, 35);
    COLORREF levelColorGold_ = RGB(255, 190, 28);
    std::chrono::steady_clock::time_point lastSlotLog_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastTopmostUpdate_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastSearchAttempt_ = {};
    std::chrono::steady_clock::time_point lastWaitingLog_ = {};

    static inline OverlayApp* instanceForLog_ = nullptr;
};
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    OverlayApp app(hInstance);
    if (!app.Init(nCmdShow))
    {
        MessageBoxW(nullptr, L"Failed to initialize Imperivm Hero Overlay.", L"Startup error", MB_OK | MB_ICONERROR);
        return 1;
    }

    return app.Run();
}
