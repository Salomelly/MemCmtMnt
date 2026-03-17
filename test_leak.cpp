#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <iostream>
#include "WarningDialog.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

// Helper to get current memory usage
SIZE_T GetWorkingSetSize()
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// Helper to get private commit usage
SIZE_T GetPrivateUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmcex;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmcex, sizeof(pmcex)))
    {
        return pmcex.PrivateUsage;
    }
    return 0;
}

void TrimWorkingSet()
{
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

// Timer callback to automatically close the message box
VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    HWND hWarning = FindWindow(NULL, L"Memory Warning");
    if (hWarning)
    {
        // Send a close message to dismiss the dialog
        PostMessage(hWarning, WM_CLOSE, 0, 0);
    }
}

void RunSingleTestIteration()
{
    // Start a timer that runs every 100ms to close the dialog
    UINT_PTR timerId = SetTimer(NULL, 1, 100, TimerProc);

    ULONGLONG snoozeUntil = 0;
    
    // Launch the child process dialog
    WarningDialog::LaunchDialogAsync(10.0, 2.0, snoozeUntil);

    // Pump messages for a brief period to allow the timer to fire 
    // and wait for the spawned dialog to be closed.
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < 1000)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(10);
        }
    }

    KillTimer(NULL, timerId);

    // After the dialog closes, call TrimWorkingSet to mimic real app behavior
    TrimWorkingSet();
}

int main()
{
    // Important: we must handle the child process invocation if it's test_leak.exe itself spawning! 
    int wargc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv)
    {
        WarningDialog::HandleDialogProcessIfRequested(wargc, wargv);
        LocalFree(wargv);
    }

    printf("--- Memory Leak Test (Multiproc Framework) ---\n");
    
    // Warm-up to load DLLs
    RunSingleTestIteration();
    Sleep(500); // Give the detached thread time to finish

    SIZE_T memStartWS = GetWorkingSetSize();
    SIZE_T memStartPrivate = GetPrivateUsage();

    printf("Baseline Working Set: %zu KB\n", memStartWS / 1024);
    printf("Baseline Private Bytes: %zu KB\n", memStartPrivate / 1024);

    const int iterations = 10;
    for (int i = 0; i < iterations; ++i)
    {
        RunSingleTestIteration();
        Sleep(200); // Give the system time to reclaim
    }

    SIZE_T memEndWS = GetWorkingSetSize();
    SIZE_T memEndPrivate = GetPrivateUsage();

    printf("Final Working Set: %zu KB\n", memEndWS / 1024);
    printf("Final Private Bytes: %zu KB\n", memEndPrivate / 1024);

    long long diffWS = (long long)memEndWS - (long long)memStartWS;
    long long diffPrivate = (long long)memEndPrivate - (long long)memStartPrivate;

    printf("Difference Working Set: %lld KB\n", diffWS / 1024);
    printf("Difference Private Bytes: %lld KB\n", diffPrivate / 1024);

    // If Private Bytes grew by more than an arbitrary threshold (e.g. 100KB), consider it a leak
    if (diffPrivate > 100 * 1024)
    {
        printf("RESULT: FAILED! Potential memory leak detected.\n");
        return 1;
    }
    
    printf("RESULT: PASSED! No substantial memory leak detected.\n");
    return 0;
}
