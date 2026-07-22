# Hero Overlay C++ for Imperivm III

A high-performance, native Win32 floating HUD Overlay written in C++ for **Imperivm III: Great Battles of Rome** (*Imperivm III: Las Grandes Batallas de la Historia*).

This mod provides a customizable real-time heads-up display (HUD Overlay) that tracks and presents Hero information, including Level, Experience progress bar, and unassigned Skill Points.

---

## 🌟 Key Features

- **High Performance & Lightweight**: Built with native C++ and Win32 GDI+ for minimal memory overhead and zero latency.
- **Customizable Experience Bar Styles**:
  - **Horizontal Blocks**: Retro/arcade style block visualization.
  - **Horizontal Reduced**: Compact style for minimal screen obstruction.
  - **Horizontal Bars**: Classic smooth fluid bar.
  - **Vertical**: Original vertical layout.
- **Hero Level Display Modes**:
  - Arabic Numerals (`1`, `2`, `3`...).
  - Roman Numerals (`I`, `II`, `III`...).
- **Visual Customization**:
  - Configurable colors for level brackets (Low, White, Brown, Gold).
  - Real-time adjustment of X/Y screen offsets and Gap Y directly from the control panel.
- **Skill Points Notification**: Visual indicator highlights when a selected hero has unspent skill points.

---

## 🛠️ Build Requirements

- **Operating System**: Windows 10 / 11 (64-bit or 32-bit).
- **IDE**: Visual Studio 2022 or later (with *Desktop development with C++* workload).
- **Compiler**: MSVC compiler supporting C++17 or later.
- **Windows SDK**: Windows 10 SDK (10.0.19041.0 or newer).

---

## 🚀 Building & Installation

### Compiling from Source

1. Clone this repository:
   ```cmd
   git clone https://github.com/Upercat/XPview-Imperivm-III-HD-Overlay.git
   ```
2. Open the solution in Visual Studio by double-clicking `hero_overlay_c++.slnx` (or `hero_overlay_c++.vcxproj`).
3. Select your target build configuration: **Release** | **x86** (or **x64** depending on your game executable).
4. Build the solution by pressing `Ctrl + Shift + B` (or *Build -> Build Solution*).
5. The compiled executable will be generated in `Release/hero_overlay_c++.exe`.

---

## 🎮 How to Use

1. Launch **Imperivm III**.
2. Run `hero_overlay_c++.exe`.
3. Customize display preferences (X/Y offsets, Experience bar mode, Arabic/Roman numerals) using the overlay controls.

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. See the [LICENSE](file:///F:/dev_Imperivm4/visual_studio/hero_overlay_c++/LICENSE) file for details.

> **GPLv3 Summary**: You are free to run, study, share, and modify this software. However, any redistributed modified versions or derivative works must also be released open source under the same GPLv3 license with accessible source code.
