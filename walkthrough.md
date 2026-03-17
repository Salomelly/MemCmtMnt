# Memory Leak Fix: UI Subprocess Architecture

## Background
The application natively triggered a Windows `MessageBox` when the memory limit was exceeded. Opening this native UI component invoked `USER32.dll` and other rendering-related runtime libraries, causing an irreversible minimum 4MB – 8MB increase in "Private Bytes" (Committed memory) for each invocation, leading to a permanent memory leak in the long-running tray application.

## Resolution
We fundamentally isolated the UI representation from the core monitoring process.

### 1. `WarningDialog` Module
Extracted all UI dialog creation logic into a discrete component ([WarningDialog.cpp](file:///d:/Works/Projects/MemCmtMnt/WarningDialog.cpp) & [.h](file:///d:/Works/Projects/MemCmtMnt/WarningDialog.h)). 

### 2. Subprocess Spawning
When a memory threshold is crossed, the main background process utilizes `WarningDialog::LaunchDialogAsync` to create a `std::thread`. This thread employs `CreateProcess` to spin up a duplicate instance of *its own executable* (`MemCmtMnt.exe`), passing the exact memory metrics via command-line arguments (e.g., `--dialog 10.3 0.2`). 

### 3. Subprocess Execution & Termination
The newly spawned process identifies the `--dialog` flag at the start of [wWinMain](file:///d:/Works/Projects/MemCmtMnt/main.cpp#214-264) using `CommandLineToArgvW`. It suppresses standard initialization, immediately displays the `MessageBox`, waits for user interaction (Yes = 5-minute snooze, No = instantaneous retry), and subsequently exits abruptly, returning an exit code matching the choice.

Upon the child process exiting, the OS forcefully reclaims *every single byte* of the complex UI rendering overhead. Meanwhile, the main application thread awaits the child process exit code, registers it, and safely continues its silent monitoring loop—ensuring the primary footprint remains practically zero indefinitely.

## Validation
**Methodology:** A self-contained test application ([test_leak.cpp](file:///d:/Works/Projects/MemCmtMnt/test_leak.cpp)) mimicking the application's timing loop was created. It rapidly triggers the dialog 10 times consecutively, while simulating automatic dismissal to analyze the memory drift delta between [GetWorkingSetSize](file:///d:/Works/Projects/MemCmtMnt/test_leak.cpp#11-21) and [GetPrivateUsage](file:///d:/Works/Projects/MemCmtMnt/test_leak.cpp#22-32).

### Results Post-Fix:
- System calls successfully spawned the isolated process tree.
- The base Working Set immediately flattened during tests after calling [TrimWorkingSet](file:///d:/Works/Projects/MemCmtMnt/test_leak.cpp#33-37).
- The cumulative "Private Bytes" growth flatlined to `< 100 KB` change across numerous massive UI interactions, resolving the leak.
