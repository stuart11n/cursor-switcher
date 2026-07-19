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
                if (IsFlightSim2024Running()) {
                    MonitorList list = { 0 };
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

                        printf("Current Monitor Index: %d\n", current_idx);
                        
                        int next_idx = (current_idx + 1) % list.count;
                        RECT target = list.rects[next_idx];
                        int centerX = target.left + (target.right - target.left) / 2;
                        int centerY = target.top + (target.bottom - target.top) / 2;

                        // 1. Force clear any clip rects
                        ClipCursor(NULL);

                        // CASE A: Leaving MSFS (Monitor 0) to go back to the Desktop (Monitor 1)
                        if (current_idx == 0) {
                            // Synthesize Alt + Escape to drop MSFS cursor lock bounds instantly
                            INPUT altEsc[4] = { 0 };
                            altEsc[0].type = INPUT_KEYBOARD; altEsc[0].ki.wVk = VK_MENU;
                            altEsc[1].type = INPUT_KEYBOARD; altEsc[1].ki.wVk = VK_ESCAPE;
                            altEsc[2].type = INPUT_KEYBOARD; altEsc[2].ki.wVk = VK_ESCAPE; altEsc[2].ki.dwFlags = KEYEVENTF_KEYUP;
                            altEsc[3].type = INPUT_KEYBOARD; altEsc[3].ki.wVk = VK_MENU;   altEsc[3].ki.dwFlags = KEYEVENTF_KEYUP;
                            SendInput(4, altEsc, sizeof(INPUT));
                            
                            Sleep(50);

                            // Move cursor to the desktop monitor center
                            SetCursorPos(centerX, centerY);
                        }
                        // CASE B: Leaving Desktop (Monitor 1) to return to MSFS (Monitor 0)
                        else if (current_idx == 1) {
                            // Find the MSFS window handle dynamically
                            HWND hwndMSFS = FindWindow(NULL, "Microsoft Flight Simulator 2024");
                            if (!hwndMSFS) {
                                hwndMSFS = FindWindow("FileDialog", "Microsoft Flight Simulator");
                            }

                            if (hwndMSFS) {
                                // Break the Windows security sandbox to force focus back onto the sim window
                                HWND hCurrForeground = GetForegroundWindow();
                                DWORD dwThisThread = GetCurrentThreadId();
                                DWORD dwVisibleThread = GetWindowThreadProcessId(hCurrForeground, NULL);
                                
                                AttachThreadInput(dwThisThread, dwVisibleThread, TRUE);
                                ShowWindow(hwndMSFS, SW_RESTORE);
                                SetForegroundWindow(hwndMSFS);
                                SetFocus(hwndMSFS);
                                AttachThreadInput(dwThisThread, dwVisibleThread, FALSE);
                            }

                            // Teleport the cursor using absolute layout coordinates
                            int totalWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                            int totalHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                            int leftBound = GetSystemMetrics(SM_XVIRTUALSCREEN);
                            int topBound = GetSystemMetrics(SM_YVIRTUALSCREEN);

                            INPUT moveInput = { 0 };
                            moveInput.type = INPUT_MOUSE;
                            moveInput.mi.dx = ((centerX - leftBound) * 65536) / totalWidth;
                            moveInput.mi.dy = ((centerY - topBound) * 65536) / totalHeight;
                            moveInput.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;
                            SendInput(1, &moveInput, sizeof(INPUT));
                            SetCursorPos(centerX, centerY);

                            // Small delay for the game process to catch the cursor coordinates
                            Sleep(30);

                            // Fire a simulated click directly into MSFS to fully reactivate its window loop
                            INPUT clickInputs[2] = { 0 };
                            clickInputs[0].type = INPUT_MOUSE;
                            clickInputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                            clickInputs[1].type = INPUT_MOUSE;
                            clickInputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                            SendInput(2, clickInputs, sizeof(INPUT));
                        }
                    }
                    return 1; // Swallow the mouse back click
                }
            }
        }
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

int main() {
    HMODULE hUser32 = GetModuleHandle("user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetDpiContextProc)(HANDLE);
        SetDpiContextProc pSetProcessDpiAwarenessContext =
            (SetDpiContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pSetProcessDpiAwarenessContext) {
            pSetProcessDpiAwarenessContext((HANDLE)-4);
        }
    }

    HWND hwnd = GetConsoleWindow();
    // ShowWindow(hwnd, SW_HIDE);

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
