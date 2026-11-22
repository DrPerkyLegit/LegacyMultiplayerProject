# LegacyMultiplayerProject

## Introduction

This project is meant to be the best multiplayer oriented version of legacy

## Build & Run

1. Install Visual Studio 2022
2. Clone the repository
3. Open the project by double-clicking `MinecraftConsoles.sln`
4. Make sure `Minecraft.Client` is set as the Startup Project
5. Set the build configuration to **Debug** or **Release** and the target platform to **Windows64**, then build and run

`Minecraft.Server` is a separate Windows64 console target. It starts the default hosted world without creating a client window or initializing the graphics device. Press Ctrl+C to stop the server. It still compiles shared client sources used by the legacy world and server simulation.
