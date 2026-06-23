# MinecraftConsoles

![img.png](img.png)

## Introduction

This project contains the source code of Minecraft Legacy Console Edition v1.3.0494.0, with some fixes applied to allow compilation and execution in Debug and Release mode on Windows using Visual Studio 2022.

## Build & Run

1. Install Visual Studio 2022
2. Clone the repository
3. Open the project by double-clicking `MinecraftConsoles.sln`
4. Make sure `Minecraft.Client` is set as the Startup Project
5. Set the build configuration to **Debug** or **Release** and the target platform to **Windows64**, then build and run

## Playing

Since Minecraft Legacy Console Edition does not natively support keyboard and mouse input, you will need to use a gamepad to play, or use a virtual gamepad emulator such as [QKeyMapper](https://github.com/Zalafina/QKeyMapper).

## Known Issues

1. Builds for other platforms have not been tested and are most likely non-functional.
2. Sound effects are missing.
3. Other unknown issues may exist.

## Goals

- [ ] Native keyboard and mouse input support
- [x] Fix compilation in Release mode on Windows
- [ ] Fix builds and execution on non-Windows platforms
