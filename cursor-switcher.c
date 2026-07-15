#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#define MAX_MONITORS 16

HHOOK hMouseHook = NULL;

typedef struct {
    RECT rects[MAX_MONITORS];
    int count;
} MonitorList;

// Check if FlightSimulator2024.exe is active in the background
BOOL IsFlightSim2024Running() {
    BOOL running = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe32)) {
            do {
                // Compare current process name against MSFS 2024 executable
                if (strcasecmp(pe32.szExeFile, "FlightSimulator2024.exe") == 0) {
                    running = TRUE;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return running;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MonitorList* list = (MonitorList*)dwData;
    if (list->count < MAX_MONITORS) {
        list->rects[list->count] = *lprcMonitor;
        list->count++;
        return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_XBUTTONDOWN) {
            MSLLHOOKSTRUCT* mouseStruct = (MSLLHOOKSTRUCT*)lParam;
            int xButton = HIWORD(mouseStruct->mouseData);
            
            if (xButton == XBUTTON1) { 
                // ONLY execute cycle if MSFS 2024 is running
                if (IsFlightSim2024Running()) {
                    ClipCursor(NULL);

                    MonitorList list = {0};
                    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&list);

                    if (list.count > 0) {
                        POINT pt;
                        GetCursorPos(&pt);

                        int current_idx = -1;
                        for (int i = 0; i < list.count; i++) {
                            if (PtInRect(&list.rects[i], pt)) {
                                current_idx = i;
                                break;
                            }
                        }

                        int next_idx = (current_idx + 1) % list.count;
                        RECT target = list.rects[next_idx];

                        int centerX = target.left + (target.right - target.left) / 2;
                        int centerY = target.top + (target.bottom - target.top) / 2;

                        SetCursorPos(centerX, centerY);
                    }
                    
                    // Prevent back click from registering inside MSFS or background apps
                    return 1; 
                }
            }
        }
    }
    // Fall back to normal mouse behavior if MSFS 2024 is closed
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

int main() {
    // Hide the console window
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_HIDE);

    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    if (!hMouseHook) {
        MessageBox(NULL, "Failed to install mouse hook!", "Error", MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMouseHook);
    return 0;
}
