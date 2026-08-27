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
#include <commctrl.h>
#include <windowsx.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Msimg32.lib")

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
constexpr COLORREF kLevelTextBackground = RGB(18, 18, 28);

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
constexpr int IDC_SLOT_MODE = 1016;
constexpr int IDC_9SLOT_PATCH_LINK = 1017;
constexpr int IDC_LEVEL_ANCHOR_X = 1018;
constexpr int IDC_LEVEL_ANCHOR_Y = 1019;
constexpr int IDC_XP_ANCHOR_X = 1022;
constexpr int IDC_XP_ANCHOR_Y = 1023;
constexpr int IDC_RESET_ANCHORS = 1024;
constexpr int IDC_LEVEL_TEXT_BACKGROUND = 1025;
constexpr int kAnchorMin = -120;
constexpr int kAnchorMax = 80;
// In placeholder.png the slot begins at x=8, while the stable overlay's
// OffsetX points to the XP bar immediately after the 52 px-wide portrait.
constexpr int kPreviewStableOverlayX = 51;
constexpr int kAnchorLayoutVersion = 2;
constexpr int kPreviewPlaceholderWidth = 206;
constexpr int kPreviewPlaceholderHeight = 95;
constexpr int kPreviewScale = 2;
constexpr int kLevelFlashMs = 800;
constexpr bool kVerboseLogging = false;
constexpr size_t kStandardSlotCount = 5;
constexpr size_t kPatchedSlotCount = 9;
constexpr size_t kMaxSlotCount = kPatchedSlotCount;
constexpr wchar_t kNineSlotsPatchUrl[] = L"https://github.com/Upercat/XPview-Imperivm-III-HD-Overlay/releases";

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

enum class SlotDisplayMode
{
    StandardFive,
    PatchedNine,
};

#if defined(_DEBUG) || defined(HERO_OVERLAY_PERF_DIAGNOSTICS)
constexpr bool kPerformanceInstrumentation = true;
#else
constexpr bool kPerformanceInstrumentation = false;
#endif

enum class PerformanceState
{
    Disconnected,
    Active,
    MapHidden,
    Minimized,
};

const char* PerformanceStateName(PerformanceState state)
{
    switch (state)
    {
    case PerformanceState::Active:
        return "active";
    case PerformanceState::MapHidden:
        return "map-hidden";
    case PerformanceState::Minimized:
        return "minimized";
    default:
        return "disconnected";
    }
}

struct TimingAccumulator
{
    uint64_t count = 0;
    uint64_t totalMicroseconds = 0;
    uint64_t maximumMicroseconds = 0;

    void Add(uint64_t microseconds)
    {
        if constexpr (kPerformanceInstrumentation)
        {
            ++count;
            totalMicroseconds += microseconds;
            maximumMicroseconds = (std::max)(maximumMicroseconds, microseconds);
        }
    }

    void Reset()
    {
        count = 0;
        totalMicroseconds = 0;
        maximumMicroseconds = 0;
    }
};

class ScopedPerformanceTimer
{
public:
    explicit ScopedPerformanceTimer(TimingAccumulator& accumulator) : accumulator_(accumulator)
    {
        if constexpr (kPerformanceInstrumentation)
        {
            started_ = std::chrono::steady_clock::now();
        }
    }

    ~ScopedPerformanceTimer()
    {
        if constexpr (kPerformanceInstrumentation)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started_);
            accumulator_.Add(static_cast<uint64_t>(elapsed.count()));
        }
    }

private:
    TimingAccumulator& accumulator_;
    std::chrono::steady_clock::time_point started_ = {};
};

class PerformanceMetrics
{
public:
    void Initialize(const std::wstring& preferencesPath)
    {
        if constexpr (!kPerformanceInstrumentation)
        {
            return;
        }

        logPath_ = std::filesystem::path(preferencesPath).parent_path() / L"hero_overlay_performance.log";
        std::ofstream log(logPath_, std::ios::trunc);
        log << "XPview performance diagnostics\n"
            << "Build: " << (kPerformanceInstrumentation ? "instrumented" : "normal") << "\n"
            << "Columns: state, CPU, Tick, ScanHeroSlots, ReadProcessMemory, PaintOverlay\n";

        reportStarted_ = std::chrono::steady_clock::now();
        lastCpuTime100ns_ = ReadProcessCpuTime100ns();
        SYSTEM_INFO systemInfo = {};
        GetSystemInfo(&systemInfo);
        processorCount_ = (std::max)(1u, static_cast<unsigned int>(systemInfo.dwNumberOfProcessors));
    }

    void SetState(PerformanceState state)
    {
        if constexpr (kPerformanceInstrumentation)
        {
            state_ = state;
        }
    }

    void RecordMemoryRead(size_t bytes, bool succeeded)
    {
        if constexpr (kPerformanceInstrumentation)
        {
            ++readCalls_;
            readBytes_ += static_cast<uint64_t>(bytes);
            if (!succeeded)
            {
                ++readFailures_;
            }
        }
    }

    void MaybeReport()
    {
        if constexpr (!kPerformanceInstrumentation)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - reportStarted_).count();
        if (seconds < 5.0)
        {
            return;
        }

        const uint64_t cpuTime100ns = ReadProcessCpuTime100ns();
        const uint64_t cpuDelta100ns = cpuTime100ns >= lastCpuTime100ns_ ? cpuTime100ns - lastCpuTime100ns_ : 0;
        const double cpuPercent = seconds > 0.0
            ? (static_cast<double>(cpuDelta100ns) / (seconds * 10000000.0 * processorCount_)) * 100.0
            : 0.0;

        std::ostringstream line;
        line << std::fixed << std::setprecision(2)
            << "[PERF] state=" << PerformanceStateName(state_)
            << " window_s=" << seconds
            << " cpu=" << cpuPercent << "%"
            << " tick={count:" << tick_.count << ",avg_us:" << Average(tick_) << ",max_us:" << tick_.maximumMicroseconds << "}"
            << " scan={count:" << scan_.count << ",avg_us:" << Average(scan_) << ",max_us:" << scan_.maximumMicroseconds << "}"
            << " rpm={calls:" << readCalls_ << ",calls_s:" << (readCalls_ / seconds)
            << ",bytes:" << readBytes_ << ",bytes_s:" << (readBytes_ / seconds) << ",failures:" << readFailures_ << "}"
            << " paint={count:" << paint_.count << ",fps:" << (paint_.count / seconds)
            << ",avg_us:" << Average(paint_) << ",max_us:" << paint_.maximumMicroseconds << "}";

        const std::string text = line.str();
        OutputDebugStringA((text + "\n").c_str());
        std::ofstream log(logPath_, std::ios::app);
        log << text << '\n';

        tick_.Reset();
        scan_.Reset();
        paint_.Reset();
        readCalls_ = 0;
        readBytes_ = 0;
        readFailures_ = 0;
        reportStarted_ = now;
        lastCpuTime100ns_ = cpuTime100ns;
    }

    TimingAccumulator tick_;
    TimingAccumulator scan_;
    TimingAccumulator paint_;

private:
    static uint64_t FileTimeToUint64(const FILETIME& fileTime)
    {
        ULARGE_INTEGER value = {};
        value.LowPart = fileTime.dwLowDateTime;
        value.HighPart = fileTime.dwHighDateTime;
        return value.QuadPart;
    }

    static uint64_t ReadProcessCpuTime100ns()
    {
        FILETIME creation = {};
        FILETIME exit = {};
        FILETIME kernel = {};
        FILETIME user = {};
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        {
            return 0;
        }
        return FileTimeToUint64(kernel) + FileTimeToUint64(user);
    }

    static double Average(const TimingAccumulator& timing)
    {
        return timing.count > 0
            ? static_cast<double>(timing.totalMicroseconds) / static_cast<double>(timing.count)
            : 0.0;
    }

    std::filesystem::path logPath_;
    std::chrono::steady_clock::time_point reportStarted_ = {};
    uint64_t lastCpuTime100ns_ = 0;
    unsigned int processorCount_ = 1;
    PerformanceState state_ = PerformanceState::Disconnected;
    uint64_t readCalls_ = 0;
    uint64_t readBytes_ = 0;
    uint64_t readFailures_ = 0;
};

PerformanceMetrics gPerformanceMetrics;

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

        const bool succeeded = ok && bytesRead == size;
        gPerformanceMetrics.RecordMemoryRead(size, succeeded);
        return succeeded;
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

    std::array<SlotInfo, kMaxSlotCount> ScanHeroSlots(size_t slotCount) const
    {
        std::array<SlotInfo, kMaxSlotCount> slots;

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

        const int lastHotkey = static_cast<int>((std::min)(slotCount, kMaxSlotCount));
        for (int hotkey = 1; hotkey <= lastHotkey; ++hotkey)
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
            if (skillLevel < -1 || skillLevel > 10)
            {
                // A torn/mismatched hero snapshot must not be rendered as dozens of '+'.
                return 0;
            }
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
            // The binary reserves a large table, but only its strictly increasing prefix is
            // populated. Reading the zero-filled tail made every positive XP value look like
            // level 1000.
            expTable_.clear();
            for (int32_t threshold : table)
            {
                if (threshold < 0 || (!expTable_.empty() && threshold <= expTable_.back()))
                {
                    break;
                }
                expTable_.push_back(threshold);
            }
            if (expTable_.size() < 2)
            {
                expTable_.clear();
            }
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

        size_t level = 0;
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
        int32_t nextLevelXp = level + 1 < expTable_.size() ? expTable_[level + 1] : currentLevelXp + 1;
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
        gPerformanceMetrics.Initialize(preferencesPath_);
        LoadPreferences();
        INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_BAR_CLASSES };
        InitCommonControlsEx(&controls);
        Gdiplus::GdiplusStartupInput gdiplusInput;
        if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusInput, nullptr) == Gdiplus::Ok)
        {
            const std::filesystem::path exeDir = std::filesystem::path(preferencesPath_).parent_path();
            const std::filesystem::path placeholderPath = exeDir.parent_path().parent_path() / L"placeholder2.png";
            placeholderPreview_ = std::make_unique<Gdiplus::Image>(placeholderPath.c_str());
            if (placeholderPreview_->GetLastStatus() != Gdiplus::Ok)
            {
                placeholderPreview_.reset();
            }
        }

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
            1100,
            520,
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
        levelTextBackgroundEnabled_ = GetPrivateProfileIntW(L"Overlay", L"LevelTextBackground", 0, preferencesPath_.c_str()) != 0;
        int savedSkillPointsStyle = static_cast<int>(GetPrivateProfileIntW(L"Overlay", L"SkillPointsStyle", 0, preferencesPath_.c_str()));
        skillPointsStyle_ = savedSkillPointsStyle == 1 ? SkillPointsStyle::ShowIndicator : SkillPointsStyle::ShowAll;
        const int savedAnchorLayoutVersion = static_cast<int>(GetPrivateProfileIntW(L"Anchors", L"LayoutVersion", 0, preferencesPath_.c_str()));
        levelAnchorX_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Anchors", L"LevelX", 8, preferencesPath_.c_str())), kAnchorMin, kAnchorMax);
        levelAnchorY_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Anchors", L"LevelY", 0, preferencesPath_.c_str())), kAnchorMin, kAnchorMax);
        xpAnchorX_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Anchors", L"XpX", 0, preferencesPath_.c_str())), kAnchorMin, kAnchorMax);
        xpAnchorY_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Anchors", L"XpY", 0, preferencesPath_.c_str())), kAnchorMin, kAnchorMax);
        // Anchor coordinates from earlier experimental previews used a different
        // reference system. Start that layout once from the stable defaults.
        if (savedAnchorLayoutVersion < kAnchorLayoutVersion)
        {
            levelAnchorX_ = 8;
            levelAnchorY_ = 0;
            xpAnchorX_ = 0;
            xpAnchorY_ = 0;
        }
        // Migrate the incorrect preview-derived preset shipped by the previous lab build.
        if (levelAnchorX_ == 66 && levelAnchorY_ == 0 && xpAnchorX_ == 58 && xpAnchorY_ == 0)
        {
            levelAnchorX_ = 8;
            xpAnchorX_ = 0;
        }
        slotDisplayMode_ = GetPrivateProfileIntW(L"Overlay", L"SlotDisplayMode", 0, preferencesPath_.c_str()) == 1
            ? SlotDisplayMode::PatchedNine
            : SlotDisplayMode::StandardFive;

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
        WritePrivateProfileStringW(L"Overlay", L"LevelTextBackground", levelTextBackgroundEnabled_ ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"SkillPointsStyle", skillPointsStyle_ == SkillPointsStyle::ShowIndicator ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Overlay", L"SlotDisplayMode", slotDisplayMode_ == SlotDisplayMode::PatchedNine ? L"1" : L"0", preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Anchors", L"LevelX", IntToWide(levelAnchorX_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Anchors", L"LevelY", IntToWide(levelAnchorY_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Anchors", L"XpX", IntToWide(xpAnchorX_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Anchors", L"XpY", IntToWide(xpAnchorY_).c_str(), preferencesPath_.c_str());
        WritePrivateProfileStringW(L"Anchors", L"LayoutVersion", IntToWide(kAnchorLayoutVersion).c_str(), preferencesPath_.c_str());
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
                gPerformanceMetrics.MaybeReport();
            }
            return 0;

        case WM_SIZE:
            LayoutControls();
            return 0;

        case WM_PAINT:
            PaintAnchorReference(hwnd);
            return 0;

        case WM_LBUTTONDOWN:
            BeginAnchorDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEMOVE:
            if (draggedAnchor_ >= 0)
            {
                UpdateAnchorDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;

        case WM_LBUTTONUP:
            if (draggedAnchor_ >= 0)
            {
                draggedAnchor_ = -1;
                ReleaseCapture();
            }
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

    static LRESULT CALLBACK AnchorEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData)
    {
        auto* app = reinterpret_cast<OverlayApp*>(refData);
        switch (message)
        {
        case WM_LBUTTONDOWN:
            app->scrubEdit_ = hwnd;
            app->scrubStartX_ = GET_X_LPARAM(lParam);
            app->scrubStartValue_ = app->ReadIntEdit(hwnd, 0);
            app->scrubMoved_ = false;
            SetCapture(hwnd);
            break;
        case WM_MOUSEMOVE:
            if (app->scrubEdit_ == hwnd && (wParam & MK_LBUTTON))
            {
                const int delta = (GET_X_LPARAM(lParam) - app->scrubStartX_) / 3;
                if (delta != 0)
                {
                    app->scrubMoved_ = true;
                    const int value = std::clamp(app->scrubStartValue_ + delta, kAnchorMin, kAnchorMax);
                    SetWindowTextW(hwnd, IntToWide(value).c_str());
                    app->ApplyAnchorControls();
                }
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (app->scrubEdit_ == hwnd)
            {
                const bool moved = app->scrubMoved_;
                app->scrubEdit_ = nullptr;
                ReleaseCapture();
                if (moved)
                {
                    return 0;
                }
            }
            break;
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    void PaintAnchorReference(HWND hwnd)
    {
        PAINTSTRUCT ps = {};
        HDC screenDc = BeginPaint(hwnd, &ps);
        constexpr int previewX = 610;
        constexpr int previewY = 110;
        constexpr int previewPaintHeight = kPreviewPlaceholderHeight * kPreviewScale + 24;
        RECT clientRect = {};
        GetClientRect(hwnd, &clientRect);
        HDC bufferDc = screenDc ? CreateCompatibleDC(screenDc) : nullptr;
        HBITMAP bufferBitmap = bufferDc ? CreateCompatibleBitmap(screenDc, clientRect.right, clientRect.bottom) : nullptr;
        HGDIOBJ oldBufferBitmap = bufferBitmap ? SelectObject(bufferDc, bufferBitmap) : nullptr;
        HDC hdc = bufferBitmap ? bufferDc : screenDc;
        if (bufferBitmap)
        {
            RECT previewArea = {
                previewX,
                previewY,
                previewX + kPreviewPlaceholderWidth * kPreviewScale,
                previewY + previewPaintHeight
            };
            FillRect(hdc, &previewArea, controlBrush_);
        }
        if (hdc && placeholderPreview_)
        {
            constexpr int scale = kPreviewScale;
            Gdiplus::Graphics graphics(hdc);
            graphics.DrawImage(placeholderPreview_.get(), previewX, previewY, kPreviewPlaceholderWidth * scale, kPreviewPlaceholderHeight * scale);

            auto marker = [&](int x, int y, COLORREF color) {
                HPEN pen = CreatePen(PS_SOLID, 2, color);
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Ellipse(hdc, x - 4, y - 4, x + 4, y + 4);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(pen);
            };

            const int originX = previewX + 8 * scale;
            const int originY = previewY + 16 * scale;
            const int savedDc = SaveDC(hdc);
            IntersectClipRect(hdc, previewX, previewY, previewX + kPreviewPlaceholderWidth * scale, previewY + kPreviewPlaceholderHeight * scale);
            marker(originX, originY, RGB(255, 255, 255));

            // Render the selected experience mode with the same routines and geometry as the overlay.
            const int experienceX = originX + (kPreviewStableOverlayX + xpAnchorX_) * scale;
            const int experienceY = originY + xpAnchorY_ * scale;
            if (expMode_ == ExperienceMode::Vertical)
            {
                HBRUSH xpBackground = CreateSolidBrush(kBarBg);
                HBRUSH xpFill = CreateSolidBrush(kBarFg);
                RECT background = { experienceX, experienceY, experienceX + kIndicatorWidth * scale, experienceY + kIndicatorHeight * scale };
                RECT fill = { experienceX, experienceY + 18 * scale, experienceX + kIndicatorWidth * scale, experienceY + kIndicatorHeight * scale };
                FillRect(hdc, &background, xpBackground);
                FillRect(hdc, &fill, xpFill);
                DeleteObject(xpFill);
                DeleteObject(xpBackground);

                PaintLevelText(hdc, 13, 1, originX + (kPreviewStableOverlayX + levelAnchorX_) * scale, originY + levelAnchorY_ * scale, 60 * scale, DT_LEFT, scale, true);
            }
            else
            {
                HBRUSH emptyBrush = CreateSolidBrush(kBlockEmpty);
                HBRUSH fillBrush = CreateSolidBrush(GetExperienceFillColor(13, false, kBlockFilled));
                const COLORREF outlineColor = levelColorsEnabled_ ? BrightenColor(GetLevelBaseColor(13), 35) : kBlockOutline;
                const int horizontalLevelX = originX + (kPreviewStableOverlayX + levelAnchorX_ - kHorizontalPaddingX) * scale;
                const int horizontalLevelY = originY + (levelAnchorY_ + kLevelTextYOffset) * scale;
                PaintLevelText(hdc, 13, 1, horizontalLevelX, horizontalLevelY, 180 * scale, DT_LEFT, scale, true);
                PaintHorizontalBlocks(hdc, emptyBrush, fillBrush, outlineColor, experienceX, experienceY, 13, 5, 10, true, expMode_, scale);
                DeleteObject(fillBrush);
                DeleteObject(emptyBrush);
            }
            RestoreDC(hdc, savedDc);
            SetTextColor(hdc, RGB(205, 214, 244));
            SetBkMode(hdc, TRANSPARENT);
            TextOutW(hdc, previewX, previewY + kPreviewPlaceholderHeight * scale, L"Preview — drag the level or XP bar to reposition it.", 52);
        }
        if (bufferBitmap)
        {
            BitBlt(
                screenDc,
                previewX,
                previewY,
                kPreviewPlaceholderWidth * kPreviewScale,
                previewPaintHeight,
                bufferDc,
                previewX,
                previewY,
                SRCCOPY);
            SelectObject(bufferDc, oldBufferBitmap);
            DeleteObject(bufferBitmap);
        }
        if (bufferDc)
        {
            DeleteDC(bufferDc);
        }
        EndPaint(hwnd, &ps);
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

        slotModeLabel_ = CreateWindowW(L"STATIC", L"Slots:", WS_CHILD | WS_VISIBLE, 10, 220, 70, 22, parent, nullptr, instance_, nullptr);
        slotModeCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 80, 216, 180, 200, parent, reinterpret_cast<HMENU>(IDC_SLOT_MODE), instance_, nullptr);
        SendMessageW(slotModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Standard (1-5)"));
        SendMessageW(slotModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1-9 Slots Patch"));
        SendMessageW(slotModeCombo_, CB_SETCURSEL, slotDisplayMode_ == SlotDisplayMode::PatchedNine ? 1 : 0, 0);
        nineSlotsPatchLink_ = CreateWindowW(
            L"BUTTON",
            L"Get the 1-9 Slots Patch",
            WS_CHILD | WS_VISIBLE | BS_FLAT,
            290,
            216,
            220,
            26,
            parent,
            reinterpret_cast<HMENU>(IDC_9SLOT_PATCH_LINK),
            instance_,
            nullptr);

        colorLowLabel_ = CreateWindowW(L"STATIC", L"<6:", WS_CHILD | WS_VISIBLE, 10, 180, 30, 22, parent, nullptr, instance_, nullptr);
        colorLowEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorLow_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 42, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_LOW), instance_, nullptr);

        colorWhiteLabel_ = CreateWindowW(L"STATIC", L"6+:", WS_CHILD | WS_VISIBLE, 128, 180, 30, 22, parent, nullptr, instance_, nullptr);
        colorWhiteEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorWhite_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 160, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_WHITE), instance_, nullptr);

        colorBrownLabel_ = CreateWindowW(L"STATIC", L"12+:", WS_CHILD | WS_VISIBLE, 246, 180, 38, 22, parent, nullptr, instance_, nullptr);
        colorBrownEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorBrown_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 288, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_BROWN), instance_, nullptr);

        colorGoldLabel_ = CreateWindowW(L"STATIC", L"24+:", WS_CHILD | WS_VISIBLE, 374, 180, 38, 22, parent, nullptr, instance_, nullptr);
        colorGoldEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ColorToHex(levelColorGold_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 416, 176, 76, 24, parent, reinterpret_cast<HMENU>(IDC_COLOR_GOLD), instance_, nullptr);

        applyButton_ = CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 415, 64, 105, 28, parent, reinterpret_cast<HMENU>(IDC_APPLY), instance_, nullptr);
        testButton_ = CreateWindowW(L"BUTTON", L"TEST OVERLAY", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 260, 140, 32, parent, reinterpret_cast<HMENU>(IDC_TEST), instance_, nullptr);
        exitButton_ = CreateWindowW(L"BUTTON", L"EXIT OVERLAY", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 380, 260, 140, 32, parent, reinterpret_cast<HMENU>(IDC_EXIT_OVERLAY), instance_, nullptr);
        anchorHintLabel_ = CreateWindowW(L"STATIC", L"Anchor offsets from the slot origin (placeholder: 8,16):", WS_CHILD | WS_VISIBLE, 10, 305, 500, 22, parent, nullptr, instance_, nullptr);
        levelAnchorLabel_ = CreateWindowW(L"STATIC", L"Level       X                         Y", WS_CHILD | WS_VISIBLE, 10, 335, 470, 22, parent, nullptr, instance_, nullptr);
        levelAnchorXEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", IntToWide(levelAnchorX_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 105, 331, 150, 24, parent, reinterpret_cast<HMENU>(IDC_LEVEL_ANCHOR_X), instance_, nullptr);
        levelAnchorYEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", IntToWide(levelAnchorY_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 300, 331, 150, 24, parent, reinterpret_cast<HMENU>(IDC_LEVEL_ANCHOR_Y), instance_, nullptr);
        xpAnchorLabel_ = CreateWindowW(L"STATIC", L"XP bar      X                         Y", WS_CHILD | WS_VISIBLE, 10, 370, 470, 22, parent, nullptr, instance_, nullptr);
        xpAnchorXEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", IntToWide(xpAnchorX_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 105, 366, 150, 24, parent, reinterpret_cast<HMENU>(IDC_XP_ANCHOR_X), instance_, nullptr);
        xpAnchorYEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", IntToWide(xpAnchorY_).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 300, 366, 150, 24, parent, reinterpret_cast<HMENU>(IDC_XP_ANCHOR_Y), instance_, nullptr);
        resetAnchorsButton_ = CreateWindowW(L"BUTTON", L"Reset anchor offsets", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 410, 180, 30, parent, reinterpret_cast<HMENU>(IDC_RESET_ANCHORS), instance_, nullptr);
        levelTextBackgroundCheck_ = CreateWindowW(L"BUTTON", L"Level text background", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 210, 413, 240, 24, parent, reinterpret_cast<HMENU>(IDC_LEVEL_TEXT_BACKGROUND), instance_, nullptr);
        SendMessageW(levelTextBackgroundCheck_, BM_SETCHECK, levelTextBackgroundEnabled_ ? BST_CHECKED : BST_UNCHECKED, 0);
        for (HWND edit : { levelAnchorXEdit_, levelAnchorYEdit_, xpAnchorXEdit_, xpAnchorYEdit_ })
        {
            SetWindowSubclass(edit, AnchorEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
        }

        std::array<HWND, 36> controls = { offsetXLabel_, offsetXEdit_, offsetYLabel_, offsetYEdit_, gapYLabel_, gapYEdit_, expModeLabel_, expModeCombo_, numberStyleLabel_, numberStyleCombo_, levelColorsCheck_, skillPointsLabel_, skillPointsCombo_, slotModeLabel_, slotModeCombo_, nineSlotsPatchLink_, colorLowLabel_, colorLowEdit_, colorWhiteLabel_, colorWhiteEdit_, colorBrownLabel_, colorBrownEdit_, colorGoldLabel_, colorGoldEdit_, applyButton_, testButton_, exitButton_, anchorHintLabel_, levelAnchorLabel_, levelAnchorXEdit_, levelAnchorYEdit_, xpAnchorLabel_, xpAnchorXEdit_, xpAnchorYEdit_, resetAnchorsButton_, levelTextBackgroundCheck_ };
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
        if (control == nineSlotsPatchLink_)
        {
            SetTextColor(hdc, RGB(137, 180, 250));
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
        MoveWindow(applyButton_, 415, rowY - 6, 105, 28, TRUE);

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

        int slotsY = rowY + 150;
        MoveWindow(slotModeLabel_, 10, slotsY, 70, 22, TRUE);
        MoveWindow(slotModeCombo_, 80, slotsY - 4, 180, 200, TRUE);
        MoveWindow(nineSlotsPatchLink_, 290, slotsY - 4, 220, 26, TRUE);

        int buttonY = rowY + 190;
        MoveWindow(testButton_, 10, buttonY, 140, 32, TRUE);
        MoveWindow(exitButton_, 380, buttonY, 140, 32, TRUE);
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

        case IDC_SLOT_MODE:
            if (notifyCode == CBN_SELCHANGE)
            {
                ApplySlotModeFromControl();
            }
            break;

        case IDC_9SLOT_PATCH_LINK:
            if (notifyCode == BN_CLICKED)
            {
                ShellExecuteW(nullptr, L"open", kNineSlotsPatchUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;

        case IDC_RESET_ANCHORS:
            ResetAnchorOffsets();
            break;

        case IDC_LEVEL_TEXT_BACKGROUND:
            if (notifyCode == BN_CLICKED)
            {
                levelTextBackgroundEnabled_ = SendMessageW(levelTextBackgroundCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                SavePreferences();
                RefreshOverlayAndPreview();
            }
            break;

        case IDC_LEVEL_ANCHOR_X:
        case IDC_LEVEL_ANCHOR_Y:
        case IDC_XP_ANCHOR_X:
        case IDC_XP_ANCHOR_Y:
            if (notifyCode == EN_CHANGE)
            {
                ApplyAnchorControls();
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

    void RefreshPreview()
    {
        constexpr int previewX = 610;
        constexpr int previewY = 110;
        RECT previewBounds = {
            previewX,
            previewY,
            previewX + kPreviewPlaceholderWidth * kPreviewScale,
            previewY + kPreviewPlaceholderHeight * kPreviewScale + 24
        };
        InvalidateRect(mainWnd_, &previewBounds, FALSE);
    }

    void RefreshOverlayAndPreview()
    {
        RefreshPreview();
        InvalidateRect(overlayWnd_, nullptr, TRUE);
    }

    void ApplyOffsetsFromControls()
    {
        offsetX_ = std::clamp(ReadIntEdit(offsetXEdit_, offsetX_), 0, 1000);
        offsetY_ = std::clamp(ReadIntEdit(offsetYEdit_, offsetY_), 0, 1000);
        gapY_ = std::clamp(ReadIntEdit(gapYEdit_, gapY_), 0, 200);
        ApplyAnchorControls();
        levelColorLow_ = ReadColorEdit(colorLowEdit_, levelColorLow_, "<6");
        levelColorWhite_ = ReadColorEdit(colorWhiteEdit_, levelColorWhite_, "6+");
        levelColorBrown_ = ReadColorEdit(colorBrownEdit_, levelColorBrown_, "12+");
        levelColorGold_ = ReadColorEdit(colorGoldEdit_, levelColorGold_, "24+");

        std::ostringstream msg;
        msg << "Settings updated - X: " << offsetX_ << ", Y: " << offsetY_ << ", Gap: " << gapY_;
        Log(msg.str());
        SavePreferences();
        RefreshOverlayAndPreview();
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
        RefreshOverlayAndPreview();
    }

    void ApplyAnchorControls()
    {
        levelAnchorX_ = std::clamp(ReadIntEdit(levelAnchorXEdit_, levelAnchorX_), kAnchorMin, kAnchorMax);
        levelAnchorY_ = std::clamp(ReadIntEdit(levelAnchorYEdit_, levelAnchorY_), kAnchorMin, kAnchorMax);
        xpAnchorX_ = std::clamp(ReadIntEdit(xpAnchorXEdit_, xpAnchorX_), kAnchorMin, kAnchorMax);
        xpAnchorY_ = std::clamp(ReadIntEdit(xpAnchorYEdit_, xpAnchorY_), kAnchorMin, kAnchorMax);
        SavePreferences();
        RefreshOverlayAndPreview();
    }

    void ResetAnchorOffsets()
    {
        // Exact stable-project defaults. OffsetX already denotes the XP bar.
        SetWindowTextW(levelAnchorXEdit_, L"8");
        SetWindowTextW(levelAnchorYEdit_, L"0");
        SetWindowTextW(xpAnchorXEdit_, L"0");
        SetWindowTextW(xpAnchorYEdit_, L"0");
        ApplyAnchorControls();
    }

    void BeginAnchorDrag(int x, int y)
    {
        constexpr int originX = 626, originY = 142;
        const int placeholderLeft = 610;
        const int placeholderTop = 110;
        const int placeholderRight = placeholderLeft + kPreviewPlaceholderWidth * kPreviewScale;
        const int placeholderBottom = placeholderTop + kPreviewPlaceholderHeight * kPreviewScale;
        if (x < placeholderLeft || x >= placeholderRight || y < placeholderTop || y >= placeholderBottom)
        {
            return;
        }

        const int barX = originX + (kPreviewStableOverlayX + xpAnchorX_) * kPreviewScale;
        const int barY = originY + xpAnchorY_ * kPreviewScale;
        if (expMode_ != ExperienceMode::Vertical)
        {
            constexpr int previewLevel = 13;
            constexpr int previewSegments = 10;
            const bool reducedMode = expMode_ == ExperienceMode::HorizontalReduced;
            const int baseWidth = reducedMode ? kHorizontalReducedBlockBaseWidth : kHorizontalBlockBaseWidth;
            const int widthLoss = reducedMode ? max(0, (previewLevel - 10) / 2) : max(0, (previewLevel - 10) / 4);
            const int segmentWidth = std::clamp(baseWidth - widthLoss, kHorizontalBlockMinWidth, baseWidth) * kPreviewScale;
            const int horizontalGap = kHorizontalGap * kPreviewScale;
            const int usedWidth = previewSegments * segmentWidth + (previewSegments - 1) * horizontalGap;
            const int horizontalBarLeft = barX + kHorizontalPaddingX * kPreviewScale;
            const int horizontalBarTop = barY + kHorizontalYOffset * kPreviewScale;
            const int horizontalBarRight = horizontalBarLeft + usedWidth;
            const int horizontalBarBottom = horizontalBarTop + kHorizontalHeight * kPreviewScale;
            if (x >= horizontalBarLeft && x < horizontalBarRight && y >= horizontalBarTop && y < horizontalBarBottom)
            {
                draggedAnchor_ = 1;
                anchorDragGrabX_ = x - barX;
                anchorDragGrabY_ = y - barY;
                SetCapture(mainWnd_);
                return;
            }
        }

        constexpr int barHitPadding = 4;
        if (expMode_ == ExperienceMode::Vertical &&
            x >= barX - barHitPadding && x < barX + kIndicatorWidth * kPreviewScale + barHitPadding &&
            y >= barY && y < barY + kIndicatorHeight * kPreviewScale)
        {
            draggedAnchor_ = 1;
            anchorDragGrabX_ = x - barX;
            anchorDragGrabY_ = y - barY;
            SetCapture(mainWnd_);
            return;
        }

        const bool horizontalMode = expMode_ != ExperienceMode::Vertical;
        const int levelX = originX + (kPreviewStableOverlayX + levelAnchorX_ - (horizontalMode ? kHorizontalPaddingX : 0)) * kPreviewScale;
        const int levelY = originY + (levelAnchorY_ + (horizontalMode ? kLevelTextYOffset : 0)) * kPreviewScale;
        if (x >= levelX && x < levelX + 60 * kPreviewScale &&
            y >= levelY && y < levelY + kLevelTextHeight * kPreviewScale)
        {
            draggedAnchor_ = 0;
            anchorDragGrabX_ = x - levelX;
            anchorDragGrabY_ = y - levelY;
            SetCapture(mainWnd_);
        }
    }

    void UpdateAnchorDrag(int x, int y)
    {
        constexpr int originX = 626, originY = 142;
        const int draggedLeft = x - anchorDragGrabX_;
        const int draggedTop = y - anchorDragGrabY_;
        const bool horizontalLevel = draggedAnchor_ == 0 && expMode_ != ExperienceMode::Vertical;
        const int horizontalLevelXCorrection = horizontalLevel ? kHorizontalPaddingX : 0;
        const int horizontalLevelYCorrection = horizontalLevel ? kLevelTextYOffset : 0;
        const int dx = std::clamp(
            (draggedLeft - originX) / kPreviewScale - kPreviewStableOverlayX + horizontalLevelXCorrection,
            kAnchorMin,
            kAnchorMax);
        const int dy = std::clamp(
            (draggedTop - originY) / kPreviewScale - horizontalLevelYCorrection,
            kAnchorMin,
            kAnchorMax);
        HWND xSlider = draggedAnchor_ == 0 ? levelAnchorXEdit_ : xpAnchorXEdit_;
        HWND ySlider = draggedAnchor_ == 0 ? levelAnchorYEdit_ : xpAnchorYEdit_;
        SetWindowTextW(xSlider, IntToWide(dx).c_str());
        SetWindowTextW(ySlider, IntToWide(dy).c_str());
        ApplyAnchorControls();
    }

    void ApplyNumberStyleFromControl()
    {
        int selected = static_cast<int>(SendMessageW(numberStyleCombo_, CB_GETCURSEL, 0, 0));
        numberStyle_ = selected == 1 ? NumberStyle::Roman : NumberStyle::Arabic;
        Log(std::string("Number style: ") + (numberStyle_ == NumberStyle::Roman ? "Roman" : "Arabic"));
        SavePreferences();
        RefreshOverlayAndPreview();
    }

    void ApplyLevelColorsFromControl()
    {
        levelColorsEnabled_ = SendMessageW(levelColorsCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        Log(std::string("Level colors: ") + (levelColorsEnabled_ ? "enabled" : "disabled"));
        SavePreferences();
        RefreshOverlayAndPreview();
    }

    void ApplySkillPointsFromControl()
    {
        int selected = static_cast<int>(SendMessageW(skillPointsCombo_, CB_GETCURSEL, 0, 0));
        skillPointsStyle_ = selected == 1 ? SkillPointsStyle::ShowIndicator : SkillPointsStyle::ShowAll;
        Log(std::string("Skill points style: ") + (skillPointsStyle_ == SkillPointsStyle::ShowIndicator ? "Show indicator" : "Show all"));
        SavePreferences();
        RefreshOverlayAndPreview();
    }

    size_t DisplayedSlotCount() const
    {
        return slotDisplayMode_ == SlotDisplayMode::PatchedNine ? kPatchedSlotCount : kStandardSlotCount;
    }

    void ApplySlotModeFromControl()
    {
        const int selected = static_cast<int>(SendMessageW(slotModeCombo_, CB_GETCURSEL, 0, 0));
        slotDisplayMode_ = selected == 1 ? SlotDisplayMode::PatchedNine : SlotDisplayMode::StandardFive;
        previousSlots_ = {};
        flashStart_ = {};
        flashUntil_ = {};
        flashSegments_.fill(1);
        flashLevel_.fill(1);
        Log(slotDisplayMode_ == SlotDisplayMode::PatchedNine
            ? "Slot mode: 1-9 Slots Patch."
            : "Slot mode: standard 1-5.");
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

    void UpdateLevelFlashState(const std::array<SlotInfo, kMaxSlotCount>& newSlots)
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
        ScopedPerformanceTimer tickTimer(gPerformanceMetrics.tick_);
        gPerformanceMetrics.SetState(
            process_.process && IsWindow(process_.hwnd)
                ? PerformanceState::Active
                : PerformanceState::Disconnected);

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
            gPerformanceMetrics.SetState(PerformanceState::Disconnected);
            DisconnectProcess();
            return;
        }

        int x = rect.left;
        int y = rect.top;
        if (x < -32000 || y < -32000)
        {
            gPerformanceMetrics.SetState(PerformanceState::Minimized);
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
        gPerformanceMetrics.SetState(mapOverlayBlocked ? PerformanceState::MapHidden : PerformanceState::Active);
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

        if (mapOverlayBlocked || !reader_)
        {
            slots_ = {};
        }
        else
        {
            ScopedPerformanceTimer scanTimer(gPerformanceMetrics.scan_);
            slots_ = reader_->ScanHeroSlots(DisplayedSlotCount());
        }
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
            for (size_t i = 0; i < DisplayedSlotCount(); ++i)
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
        ScopedPerformanceTimer paintTimer(gPerformanceMetrics.paint_);
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

        for (size_t i = 0; i < DisplayedSlotCount(); ++i)
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
                // Horizontal modes keep the stable default geometry, while allowing
                // the level and experience bar to be positioned independently.
                PaintLevelText(
                    paintDc,
                    displayLevel,
                    displaySkillPoints,
                    offsetX_ + levelAnchorX_ - kHorizontalPaddingX,
                    yStart + levelAnchorY_ + kLevelTextYOffset,
                    180,
                    DT_LEFT);
                PaintHorizontalBlocks(paintDc, blockEmptyBrush, fillBrush, outlineColor, offsetX_ + xpAnchorX_, yStart + xpAnchorY_, displayLevel, displayXpInLevel, displayXpNeeded, !flashing || flashBlinkOn, expMode_);
                DeleteObject(fillBrush);
                continue;
            }

            PaintLevelText(paintDc, displayLevel, displaySkillPoints, offsetX_ + levelAnchorX_, yStart + levelAnchorY_, 90, DT_LEFT);

            RECT bg = { offsetX_ + xpAnchorX_, yStart + xpAnchorY_, offsetX_ + xpAnchorX_ + kIndicatorWidth, yStart + xpAnchorY_ + kIndicatorHeight };
            FillRect(paintDc, &bg, barBgBrush);

            int barHeight = static_cast<int>((displayPct / 100.0) * kIndicatorHeight);
            RECT fg = {
                offsetX_ + xpAnchorX_,
                yStart + xpAnchorY_ + kIndicatorHeight - barHeight,
                offsetX_ + xpAnchorX_ + kIndicatorWidth,
                yStart + xpAnchorY_ + kIndicatorHeight
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

    void PaintLevelTextBackground(HDC hdc, const RECT& rect, bool previewSurface) const
    {
        if (previewSurface)
        {
            HDC sourceDc = CreateCompatibleDC(hdc);
            HBITMAP sourceBitmap = CreateCompatibleBitmap(hdc, 1, 1);
            HGDIOBJ oldBitmap = sourceBitmap ? SelectObject(sourceDc, sourceBitmap) : nullptr;
            if (sourceDc && sourceBitmap)
            {
                SetPixelV(sourceDc, 0, 0, kLevelTextBackground);
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 150, 0 };
                AlphaBlend(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, sourceDc, 0, 0, 1, 1, blend);
            }
            if (oldBitmap)
            {
                SelectObject(sourceDc, oldBitmap);
            }
            if (sourceBitmap)
            {
                DeleteObject(sourceBitmap);
            }
            if (sourceDc)
            {
                DeleteDC(sourceDc);
            }
            return;
        }

        // The game overlay uses a color key rather than per-pixel alpha. A fine
        // checker pattern keeps half of the game pixels visible behind the label.
        HDC patternDc = CreateCompatibleDC(hdc);
        HBITMAP patternBitmap = CreateCompatibleBitmap(hdc, 2, 2);
        HGDIOBJ oldBitmap = patternBitmap ? SelectObject(patternDc, patternBitmap) : nullptr;
        if (patternDc && patternBitmap)
        {
            SetPixelV(patternDc, 0, 0, kLevelTextBackground);
            SetPixelV(patternDc, 1, 0, kTransparentColor);
            SetPixelV(patternDc, 0, 1, kTransparentColor);
            SetPixelV(patternDc, 1, 1, kLevelTextBackground);
            HBRUSH patternBrush = CreatePatternBrush(patternBitmap);
            if (patternBrush)
            {
                FillRect(hdc, &rect, patternBrush);
                DeleteObject(patternBrush);
            }
        }
        if (oldBitmap)
        {
            SelectObject(patternDc, oldBitmap);
        }
        if (patternBitmap)
        {
            DeleteObject(patternBitmap);
        }
        if (patternDc)
        {
            DeleteDC(patternDc);
        }
    }

    void PaintLevelText(HDC hdc, int level, int availableSkillPoints, int x, int y, int width, UINT align, int renderScale = 1, bool previewSurface = false) const
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
        renderScale = max(1, renderScale);
        RECT levelRect = { x, y, x + width, y + kLevelTextHeight * renderScale };
        HFONT font = CreateFontW(
            kLevelFontHeight * renderScale,
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
        if (levelTextBackgroundEnabled_)
        {
            SIZE textSize = {};
            if (GetTextExtentPoint32W(hdc, levelText.c_str(), static_cast<int>(levelText.size()), &textSize))
            {
                const int paddingX = 3 * renderScale;
                const int paddingY = 2 * renderScale;
                RECT backgroundRect = {
                    x - paddingX,
                    y + paddingY,
                    x + textSize.cx + paddingX,
                    y + kLevelTextHeight * renderScale - paddingY
                };
                PaintLevelTextBackground(hdc, backgroundRect, previewSurface);
            }
        }
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

    void PaintHorizontalBlocks(HDC hdc, HBRUSH emptyBrush, HBRUSH filledBrush, COLORREF outlineColor, int x, int yStart, int level, int xpInLevel, int xpNeeded, bool showFilledBlocks, ExperienceMode expMode, int renderScale = 1)
    {
        renderScale = max(1, renderScale);
        int blockX = x + kHorizontalPaddingX * renderScale;
        int y = yStart + kHorizontalYOffset * renderScale;
        int segments = max(1, xpNeeded);
        int filledSegments = std::clamp(xpInLevel, 0, segments);
        int gaps = segments - 1;
        bool reducedMode = expMode == ExperienceMode::HorizontalReduced;
        bool barsMode = expMode == ExperienceMode::HorizontalBars;
        int baseWidth = (reducedMode) ? kHorizontalReducedBlockBaseWidth : kHorizontalBlockBaseWidth;
        int widthLoss = (reducedMode) ? max(0, (level - 10) / 2) : max(0, (level - 10) / 4);
        int segmentWidth = std::clamp(baseWidth - widthLoss, kHorizontalBlockMinWidth, baseWidth) * renderScale;
        int horizontalGap = (level > 40 ? 2 : kHorizontalGap) * renderScale;
        int usedWidth = segments * segmentWidth + gaps * horizontalGap;

        HPEN outlinePen = CreatePen(PS_SOLID, renderScale, outlineColor);
        HGDIOBJ oldPen = SelectObject(hdc, outlinePen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        bool continuousBar = barsMode || (reducedMode && level >= 50);

        if (continuousBar)
        {
            RECT block = {
                blockX,
                y,
                blockX + usedWidth,
                y + kHorizontalHeight * renderScale
            };
            FillRect(hdc, &block, emptyBrush);
            int filledWidth = (filledSegments * usedWidth) / segments;
            if (showFilledBlocks && filledWidth > 0)
            {
                RECT filledBlock = {
                    blockX,
                    y,
                    blockX + filledWidth,
                    y + kHorizontalHeight * renderScale
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
                    y + kHorizontalHeight * renderScale
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
        placeholderPreview_.reset();
        if (gdiplusToken_)
        {
            Gdiplus::GdiplusShutdown(gdiplusToken_);
            gdiplusToken_ = 0;
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
    HWND slotModeLabel_ = nullptr;
    HWND slotModeCombo_ = nullptr;
    HWND nineSlotsPatchLink_ = nullptr;
    HWND anchorHintLabel_ = nullptr;
    HWND levelAnchorLabel_ = nullptr;
    HWND levelAnchorXEdit_ = nullptr;
    HWND levelAnchorYEdit_ = nullptr;
    HWND xpAnchorLabel_ = nullptr;
    HWND xpAnchorXEdit_ = nullptr;
    HWND xpAnchorYEdit_ = nullptr;
    HWND resetAnchorsButton_ = nullptr;
    HWND levelTextBackgroundCheck_ = nullptr;
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
    ULONG_PTR gdiplusToken_ = 0;
    std::unique_ptr<Gdiplus::Image> placeholderPreview_;
    GameProcess process_;
    std::unique_ptr<GameMemoryReader> reader_;
    std::array<SlotInfo, kMaxSlotCount> slots_;
    std::array<SlotInfo, kMaxSlotCount> previousSlots_;
    std::array<std::chrono::steady_clock::time_point, kMaxSlotCount> flashStart_ = {};
    std::array<std::chrono::steady_clock::time_point, kMaxSlotCount> flashUntil_ = {};
    std::array<int, kMaxSlotCount> flashSegments_ = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    std::array<int, kMaxSlotCount> flashLevel_ = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    std::wstring preferencesPath_;
    int offsetX_ = 60;
    int offsetY_ = 88;
    int gapY_ = 68;
    int levelAnchorX_ = 8;
    int levelAnchorY_ = 0;
    int xpAnchorX_ = 0;
    int xpAnchorY_ = 0;
    int draggedAnchor_ = -1;
    int anchorDragGrabX_ = 0;
    int anchorDragGrabY_ = 0;
    HWND scrubEdit_ = nullptr;
    int scrubStartX_ = 0;
    int scrubStartValue_ = 0;
    bool scrubMoved_ = false;
    int lastX_ = -1;
    int lastY_ = -1;
    int lastWidth_ = -1;
    int lastHeight_ = -1;
    bool testMode_ = false;
    bool overlayVisible_ = true;
    ExperienceMode expMode_ = ExperienceMode::Vertical;
    NumberStyle numberStyle_ = NumberStyle::Arabic;
    SkillPointsStyle skillPointsStyle_ = SkillPointsStyle::ShowAll;
    SlotDisplayMode slotDisplayMode_ = SlotDisplayMode::StandardFive;
    bool levelColorsEnabled_ = false;
    bool levelTextBackgroundEnabled_ = false;
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
