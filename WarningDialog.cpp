#include "WarningDialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <string>

#pragma comment(lib, "user32.lib")

namespace WarningDialog
{
    void HandleDialogProcessIfRequested(int argc, LPWSTR* argv)
    {
        if (argc >= 4 && wcscmp(argv[1], L"--dialog") == 0)
        {
            double usedGB = _wtof(argv[2]);
            double remainGB = _wtof(argv[3]);

            wchar_t msg[256];
            swprintf_s(msg, 256, 
                L"Committed memory is near the limit!\n\n"
                L"Committed: %.2f GB\n"
                L"Remaining: %.2f GB\n\n"
                L"Snooze warnings for 5 minutes?", 
                usedGB, remainGB);

            // Display the message box
            int res = MessageBox(NULL, msg, L"Memory Warning", MB_ICONWARNING | MB_YESNO | MB_TOPMOST);
            
            // Exit code 1 means Yes (Snooze), 0 means No
            ExitProcess(res == IDYES ? 1 : 0);
        }
    }

    void LaunchDialogAsync(double usedGB, double remainGB, ULONGLONG& snoozeUntil)
    {
        // Construct command line for the child process
        wchar_t exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);

        wchar_t cmdLine[MAX_PATH + 100];
        swprintf_s(cmdLine, MAX_PATH + 100, L"\"%s\" --dialog %.2f %.2f", exePath, usedGB, remainGB);

        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            // Successfully created child process
            // We need to wait for it asynchronously to not block the main tray loop
            
            // Capture a pointer to the snoozeUntil variable
            ULONGLONG* pSnooze = &snoozeUntil;

            std::thread([pi, pSnooze]() {
                WaitForSingleObject(pi.hProcess, INFINITE);
                
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);

                if (exitCode == 1)
                {
                    *pSnooze = GetTickCount64() + 5 * 60 * 1000;
                }
                else
                {
                    *pSnooze = 0; // Not snoozed or error
                }

                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }).detach();
        }
    }
}
