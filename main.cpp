#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include "WarningDialog.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_EXIT 1001
#define ID_TOGGLE_MONITOR 1002
#define ID_INTERVAL_1S 1003
#define ID_INTERVAL_5S 1004
#define ID_INTERVAL_10S 1005
#define ID_THRESHOLD_500M 1006
#define ID_THRESHOLD_1G 1007
#define ID_THRESHOLD_2G 1008

#ifdef _DEBUG
#define ID_DEBUG_TRIGGER 1009
#endif

// ===== 配置 =====
static int g_threshold_mb = 1024;
static int g_interval_ms = 1000;
static bool g_monitor_enabled = true;
// =================

static bool g_warning = false;
static bool g_in_msgbox = false;
static ULONGLONG g_snooze_until = 0;
static HANDLE g_mutex = NULL;
static NOTIFYICONDATA g_nid;

// -------- 单实例 --------
BOOL EnsureSingleInstance()
{
    g_mutex = CreateMutex(NULL, TRUE, L"CommitMonitorLight");
    return (GetLastError() != ERROR_ALREADY_EXISTS);
}

// -------- 瘦身进程内存 --------
void TrimWorkingSet()
{
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

// -------- 自启动注册 --------
void SetupAutoStart()
{
#ifndef _DEBUG
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegSetValueEx(hKey, L"CommitMonitorLight", 0, REG_SZ, 
            (BYTE*)path, (lstrlen(path) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
#endif
}

// -------- 提取提示框 --------
// Removed ShowWarningDialog, moved to WarningDialog.cpp

// -------- 检测内存 --------
void CheckMemory()
{
    if (g_in_msgbox)
        return;

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);

    if (!GlobalMemoryStatusEx(&mem))
        return;

    SIZE_T remainMB = mem.ullAvailPageFile / (1024 * 1024);

    if (remainMB < g_threshold_mb)
    {
        bool just_crossed = !g_warning;
        bool snooze_expired = (g_snooze_until != 0 && GetTickCount64() >= g_snooze_until);

        if (just_crossed)
        {
            g_warning = true;

            g_nid.uFlags = NIF_ICON;
            g_nid.hIcon = LoadIcon(NULL, IDI_ERROR);
            Shell_NotifyIcon(NIM_MODIFY, &g_nid);
        }

        if ((just_crossed || snooze_expired) && GetTickCount64() >= g_snooze_until)
        {
            double usedGB = (double)(mem.ullTotalPageFile - mem.ullAvailPageFile) / (1024.0 * 1024.0 * 1024.0);
            double remainGB = (double)mem.ullAvailPageFile / (1024.0 * 1024.0 * 1024.0);
            WarningDialog::LaunchDialogAsync(usedGB, remainGB, g_snooze_until);
        }
    }
    else
    {
        if (g_warning)
        {
            g_warning = false;
            g_nid.uFlags = NIF_ICON;
            g_nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
            Shell_NotifyIcon(NIM_MODIFY, &g_nid);
        }
    }
}

// -------- 窗口过程 --------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON && lParam == WM_RBUTTONUP)
    {
        POINT pt;
        GetCursorPos(&pt);

        HMENU menu = CreatePopupMenu();
        
        UINT toggleFlags = MF_STRING;
        if (g_monitor_enabled) toggleFlags |= MF_CHECKED;
        AppendMenu(menu, toggleFlags, ID_TOGGLE_MONITOR, L"Enable Monitor");

        HMENU intervalMenu = CreatePopupMenu();
        AppendMenu(intervalMenu, MF_STRING | (g_interval_ms == 1000 ? MF_CHECKED : 0), ID_INTERVAL_1S, L"1 Sec");
        AppendMenu(intervalMenu, MF_STRING | (g_interval_ms == 5000 ? MF_CHECKED : 0), ID_INTERVAL_5S, L"5 Sec");
        AppendMenu(intervalMenu, MF_STRING | (g_interval_ms == 10000 ? MF_CHECKED : 0), ID_INTERVAL_10S, L"10 Sec");
        AppendMenu(menu, MF_POPUP, (UINT_PTR)intervalMenu, L"Interval");

        HMENU thresholdMenu = CreatePopupMenu();
        AppendMenu(thresholdMenu, MF_STRING | (g_threshold_mb == 500 ? MF_CHECKED : 0), ID_THRESHOLD_500M, L"500 MB");
        AppendMenu(thresholdMenu, MF_STRING | (g_threshold_mb == 1024 ? MF_CHECKED : 0), ID_THRESHOLD_1G, L"1 GB");
        AppendMenu(thresholdMenu, MF_STRING | (g_threshold_mb == 2048 ? MF_CHECKED : 0), ID_THRESHOLD_2G, L"2 GB");
        AppendMenu(menu, MF_POPUP, (UINT_PTR)thresholdMenu, L"Threshold");

#ifdef _DEBUG
        AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        AppendMenu(menu, MF_STRING, ID_DEBUG_TRIGGER, L"Force Trigger Alert (Debug)");
#endif

        AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        AppendMenu(menu, MF_STRING, ID_EXIT, L"Exit");

        SetForegroundWindow(hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON,
            pt.x, pt.y, 0, hwnd, NULL);

        // Destroy the root menu; child submenus are destroyed automatically
        DestroyMenu(menu);
    }
    else if (msg == WM_COMMAND)
    {
        WORD id = LOWORD(wParam);
        if (id == ID_EXIT)
        {
            PostQuitMessage(0);
        }
        else if (id == ID_TOGGLE_MONITOR)
        {
            g_monitor_enabled = !g_monitor_enabled;
        }
        else if (id == ID_INTERVAL_1S || id == ID_INTERVAL_5S || id == ID_INTERVAL_10S)
        {
            if (id == ID_INTERVAL_1S) g_interval_ms = 1000;
            else if (id == ID_INTERVAL_5S) g_interval_ms = 5000;
            else if (id == ID_INTERVAL_10S) g_interval_ms = 10000;

            SetTimer(hwnd, 1, g_interval_ms, NULL);
        }
        else if (id == ID_THRESHOLD_500M || id == ID_THRESHOLD_1G || id == ID_THRESHOLD_2G)
        {
            if (id == ID_THRESHOLD_500M) g_threshold_mb = 500;
            else if (id == ID_THRESHOLD_1G) g_threshold_mb = 1024;
            else if (id == ID_THRESHOLD_2G) g_threshold_mb = 2048;
            
            // Trigger check immediately on threshold change
            CheckMemory();
        }
#ifdef _DEBUG
        else if (id == ID_DEBUG_TRIGGER)
        {
            MEMORYSTATUSEX mem;
            mem.dwLength = sizeof(mem);
            if (GlobalMemoryStatusEx(&mem))
            {
                double usedGB = (double)(mem.ullTotalPageFile - mem.ullAvailPageFile) / (1024.0 * 1024.0 * 1024.0);
                double remainGB = (double)mem.ullAvailPageFile / (1024.0 * 1024.0 * 1024.0);
                WarningDialog::LaunchDialogAsync(usedGB, remainGB, g_snooze_until);
            }
        }
#endif
    }
    else if (msg == WM_TIMER)
    {
        if (g_monitor_enabled)
        {
            CheckMemory();
        }
    }
    else if (msg == WM_DESTROY)
    {
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------- WinMain --------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR pCmdLine, int)
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv)
    {
        WarningDialog::HandleDialogProcessIfRequested(argc, argv);
        LocalFree(argv);
    }

    if (!EnsureSingleInstance())
        return 0;

    SetupAutoStart();

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MemLight";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"MemLight", L"",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, 0, hInst, 0);

    // 托盘图标初始化
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    lstrcpy(g_nid.szTip, L"Commit Monitor");

    Shell_NotifyIcon(NIM_ADD, &g_nid);

    SetTimer(hwnd, 1, g_interval_ms, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_mutex) CloseHandle(g_mutex);
    return 0;
}