#pragma once

#include <windows.h>

namespace WarningDialog
{
    // Check if the current process was launched with --dialog.
    // If so, it displays the dialog and exits the process with the snooze result.
    void HandleDialogProcessIfRequested(int argc, LPWSTR* argv);

    // Launch the dialog in a child process asynchronously.
    void LaunchDialogAsync(double usedGB, double remainGB, ULONGLONG& snoozeUntil);
}
