#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <vector>
#include <string>
#include "resource.h"
#include <commctrl.h> 

#pragma comment(lib, "comctl32.lib") 
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

struct AppWindow {
    HWND hwnd;
    std::wstring processName;
    std::wstring windowTitle;
};

NOTIFYICONDATA g_nid = { 0 };
std::vector<AppWindow> g_detectedApps;

HINSTANCE g_hInstance = NULL; // Stockera l'instance réelle de l'application

// Procédure de la boîte de dialogue À propos
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        // --- CALCUL DU CENTRAGE SUR L'ÉCRAN PRINCIPAL ---
        RECT rcDialog, rcScreen;

        // 1. Récupérer les dimensions de la boîte de dialogue
        GetWindowRect(hDlg, &rcDialog);
        int width = rcDialog.right - rcDialog.left;
        int height = rcDialog.bottom - rcDialog.top;

        // 2. Récupérer les dimensions utiles de l'écran principal (exclut la barre des tâches)
        HMONITOR hPrimaryMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfoW(hPrimaryMonitor, &mi)) {
            rcScreen = mi.rcWork; // rcWork garantit qu'on ne chevauche pas la barre des tâches
        }
        else {
            // Sécurité si l'API échoue : on prend les valeurs brutes du système
            rcScreen.left = 0;
            rcScreen.top = 0;
            rcScreen.right = GetSystemMetrics(SM_CXSCREEN);
            rcScreen.bottom = GetSystemMetrics(SM_CYSCREEN);
        }

        // 3. Calculer les coordonnées X et Y pour un centrage parfait
        int screenWidth = rcScreen.right - rcScreen.left;
        int screenHeight = rcScreen.bottom - rcScreen.top;

        int x = rcScreen.left + (screenWidth - width) / 2;
        int y = rcScreen.top + (screenHeight - height) / 2;

        // 4. Déplacer la boîte de dialogue au centre
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

        return (INT_PTR)TRUE;
    }

    case WM_NOTIFY: {
        LPNMHDR pNmhdr = (LPNMHDR)lParam;
        if (pNmhdr->code == NM_CLICK || pNmhdr->code == NM_RETURN) {
            PNMLINK pNmlink = (PNMLINK)lParam;
            ShellExecuteW(NULL, L"open", pNmlink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
            return (INT_PTR)TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

    int titleLength = GetWindowTextLengthW(hwnd);
    if (titleLength == 0) return TRUE;

    std::vector<wchar_t> titleBuffer(titleLength + 1);
    GetWindowTextW(hwnd, titleBuffer.data(), titleLength + 1);
    std::wstring windowTitle(titleBuffer.data());

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    std::wstring processName = L"Inconnu";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        wchar_t procNameBuffer[MAX_PATH];
        if (GetModuleBaseNameW(hProcess, NULL, procNameBuffer, MAX_PATH)) {
            processName = procNameBuffer;
        }
        CloseHandle(hProcess);
    }

    g_detectedApps.push_back({ hwnd, processName, windowTitle });
    return TRUE;
}

void RapatrierSurEcranPrincipal(HWND targetHwnd) {
    HMONITOR hPrimaryMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (GetMonitorInfo(hPrimaryMonitor, &mi)) {
        RECT winRect;
        if (GetWindowRect(targetHwnd, &winRect)) {
            int width = winRect.right - winRect.left;
            int height = winRect.bottom - winRect.top;
            int newX = mi.rcWork.left;
            int newY = mi.rcWork.top;

            if (IsZoomed(targetHwnd) || IsIconic(targetHwnd)) {
                ShowWindow(targetHwnd, SW_RESTORE);
            }

            SetWindowPos(targetHwnd, HWND_TOP, newX, newY, width, height, SWP_SHOWWINDOW);
            SetForegroundWindow(targetHwnd);
        }
    }
}

bool EstAuDemarrage() {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        lRes = RegQueryValueExW(hKey, L"GetMyWindowBack", NULL, NULL, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return (lRes == ERROR_SUCCESS);
    }
    return false;
}

void DefinirDemarrage(bool activer) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (lRes == ERROR_SUCCESS) {
        if (activer) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            RegSetValueExW(hKey, L"GetMyWindowBack", 0, REG_SZ, (BYTE*)szPath, (DWORD)((wcslen(szPath) + 1) * sizeof(wchar_t)));
        }
        else {
            RegDeleteValueW(hKey, L"GetMyWindowBack");
        }
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);

            g_detectedApps.clear();
            EnumWindows(EnumWindowsProc, 0);

            HMENU hMenu = CreatePopupMenu();

            for (size_t i = 0; i < g_detectedApps.size(); ++i) {
                std::wstring menuText = L"[" + g_detectedApps[i].processName + L"] " + g_detectedApps[i].windowTitle;
                if (menuText.length() > 60) menuText = menuText.substr(0, 57) + L"...";
                AppendMenuW(hMenu, MF_STRING, ID_APP_START_RANGE + i, menuText.c_str());
            }

            if (g_detectedApps.empty()) {
                AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, L"(Aucune application d\u00E9tect\u00E9e)");
            }

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);

            HMENU hMenu = CreatePopupMenu();

            UINT flags = MF_STRING;
            if (EstAuDemarrage()) {
                flags |= MF_CHECKED;
            }

            AppendMenuW(hMenu, flags, ID_TRAY_STARTUP, L"Lancer au d\u00E9marrage");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT, L"\u00C0 propos");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quitter le Sauveteur");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        if (wmId == ID_TRAY_EXIT) {
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
        }
        else if (wmId == ID_TRAY_STARTUP) {
            bool etatActuel = EstAuDemarrage();
            DefinirDemarrage(!etatActuel);
        }
        else if (wmId == ID_TRAY_ABOUT) {
            // Ouvre la boîte de dialogue avec les liens cliquables
            const auto result = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT_DIALOG), hwnd, AboutDlgProc);
            // 2. Si le résultat vaut -1, la boîte a planté à l'initialisation
            if (result == -1) {
                DWORD error = GetLastError();
                wchar_t diagnostic[256];

                // Code 1813 = L'ID de la ressource (IDD_ABOUT_DIALOG) n'est pas trouvé dans le .rc
                // Code 1407 = La classe de contrôle (LinkWindow) n'est pas enregistrée
                swprintf_s(diagnostic,
                    L"Erreur Win32 fatale !\n\n"
                    L"La bo\u00EEte de dialogue n'a pas pu s'ouvrir.\n"
                    L"Code d'erreur renvoy\u00E9 par Windows : %lu",
                    error);

                MessageBoxW(hwnd, diagnostic, L"Diagnostic Sauveteur", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            }
        }
        else if (wmId >= ID_APP_START_RANGE && wmId < ID_APP_START_RANGE + (int)g_detectedApps.size()) {
            int index = wmId - ID_APP_START_RANGE;
            RapatrierSurEcranPrincipal(g_detectedApps[index].hwnd);
        }
        break;
    }

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // -------------------------------------------------------------------------
    // CRUCIAL : Initialisation de la classe de fenêtres SysLink (LinkWindow)
    // -------------------------------------------------------------------------
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LINK_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"TrayListenerWindowClass";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"TrayListener", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;

    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    g_nid.hIcon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_MY_TRAY_ICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    );

    wcscpy_s(g_nid.szTip, L"Sauveteur de fen\u00EAtres fant\u00F4mes");
    Shell_NotifyIcon(NIM_ADD, &g_nid);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
