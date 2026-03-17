# AGENTS.md

This file provides context and instructions for AI coding agents working on this project.

## Project overview
- **Language**: C++
- **Build System**: Visual Studio / MSBuild (*.sln, *.vcxproj)

## Agent Guidelines
1. Ensure code compiles cleanly with Visual Studio C++ toolchains.
2. Stick to modern C++ conventions and best practices.
3. Keep the code properly formatted.
4. When writing code, prefer descriptive naming conventions and add adequate comments for complex logic.
5. **CRITICAL: Memory Leaks & Native UI.** Do NOT invoke native USER32 UI elements (like `MessageBox`) directly within the main resident tray application loop. Due to how modern Windows lazyloads UI rendering dlls, it causes irreversible memory footprint scaling. Always use the subprocess proxy pattern (via `WarningDialog`) for any UI interactions to ensure working set and private page bounds remain clamped near 1MB.

## Code Navigation
- **main.cpp**: Program entry point and silent system memory monitor background loop.
- **WarningDialog.cpp / .h**: Isolated UI representation. It is designed to act as a stand-alone subprocess wrapper ensuring graceful dialog cleanups.
- **test_leak.cpp**: An automated testbed evaluating the memory footprint of the underlying application logic and verifying the absence of UX memory leaks.
