# 50 Cent: Bulletproof - XBE Recomp

An unofficial native PC port of the original Xbox version of 50 Cent: Bulletproof, with extended graphics options and other enhancements.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation

- Put your game files in the `assets` folder. This can be an ISO, or the extracted data files.
- Run `Bulletproof.exe`.
- Recomp settings can be adjusted in `settings.toml` beside the exe.

> [!WARNING]
> Early work in progress: the game boots, plays its movies and front end, and reaches the first mission. Later missions
> have not been played through yet.

## Features

### Display
- **60 FPS** - Runs at 60 frames a second (the console ran at 30); the game's simulation follows the clock, so it plays at its own speed.

### Controller
Supports Xbox, PlayStation and Switch controllers, with hot plugging and rumble.
- **Modern Button Prompts** - The prompts show today's Xbox controller: LB and RB for White and Black, LT and RT, View and Menu, coloured A B X Y. Can be turned off (`modern_buttons`).

### Keyboard and Mouse
- **Keyboard and Mouse** - WASD, and the mouse turns the camera directly as in a PC shooter (not as a stick), with the prompts showing the keys while you play on the keyboard.
- **Mouse in the Menus** - The left button selects, the right goes back, the wheel moves up and down.

### General
- **PC Settings in the Options page** - Y on the game's Options page (from the title and the pause menu) opens the PC settings; F1 or Back + Start anywhere.

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.

## Controls

| Action | Controller | Keyboard and mouse |
|---|---|---|
| Move | Left stick | W A S D |
| Camera, aim | Right stick | Mouse |
| Fire | RT | Left mouse button |
| Crouch | LT | C or Ctrl |
| Jump | A | Space |
| Hostage, execute, grab cover | B | F |
| Action, reload, counter-kill, cover | X | E or R |
| Evade, dive, dual-wield | Y | Shift |
| Heal | LB | H |
| Weapon inventory | RB | Q or middle mouse button |
| Music track | View | Tab |
| Pause (the G-Unit mobile) | Menu | Esc or P |
| Menus: select, back | A, B | Enter or left click, Backspace or right click |

## System Requirements

### Minimum
- **OS**
  - Windows 10 64-bit
- **GPU** with support for Direct3D 12

## Building

### 1. Clone the Repository
Clone this repository using [Git](https://git-scm.com/).

### 2. Add the Required Game Files
Place your extracted game data files in the `assets` folder. `default.xbe` must be there, with the game's folders beside it.

### 3. Install Dependencies
Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (`build.bat` offers to add the last two).

### 4. Build the Project
Run `build.bat`. The first build recompiles the game and takes a while. The exe is written to `out\build\release`.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp), the recompiler and runtime this is built on.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
