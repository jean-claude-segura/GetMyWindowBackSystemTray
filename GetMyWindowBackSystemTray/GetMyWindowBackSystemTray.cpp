#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <vector>
#include <string>
#include "resource.h"

// Structure pour stocker les fenêtres détectées
struct AppWindow {
    HWND hwnd;
    std::wstring processName;
    std::wstring windowTitle;
};

// Variables globales pour gérer l'état de l'application Win32
NOTIFYICONDATA g_nid = { 0 };
std::vector<AppWindow> g_detectedApps;

// Rappel d'énumération : capture les vraies fenêtres applicatives visibles
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

// Fonction de rapatriement (Version identique à la console : coin supérieur gauche)
void RapatrierSurEcranPrincipal(HWND targetHwnd) {
    // Trouver le moniteur principal actif
    HMONITOR hPrimaryMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (GetMonitorInfo(hPrimaryMonitor, &mi)) {
        RECT winRect;
        if (GetWindowRect(targetHwnd, &winRect)) {
            // On conserve strictement les dimensions actuelles de la fenêtre
            int width = winRect.right - winRect.left;
            int height = winRect.bottom - winRect.top;

            // Coordonnées de destination : le coin supérieur gauche de la zone de travail (rcWork.left, rcWork.top)
            int newX = mi.rcWork.left;
            int newY = mi.rcWork.top;

            // Forcer la restauration si elle est maximisée ou réduite
            if (IsZoomed(targetHwnd) || IsIconic(targetHwnd)) {
                ShowWindow(targetHwnd, SW_RESTORE);
            }

            // Déplacement identique à la version console
            SetWindowPos(targetHwnd, HWND_TOP, newX, newY, width, height, SWP_SHOWWINDOW);
            SetForegroundWindow(targetHwnd);
        }
    }
}

// Vérifie si l'application est configurée pour démarrer avec Windows
bool EstAuDemarrage() {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        // On vérifie si la valeur "GetMyWindowBack" existe
        lRes = RegQueryValueExW(hKey, L"GetMyWindowBack", NULL, NULL, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return (lRes == ERROR_SUCCESS);
    }
    return false;
}

// Active ou désactive le lancement automatique
void DefinirDemarrage(bool activer) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (lRes == ERROR_SUCCESS) {
        if (activer) {
            wchar_t szPath[MAX_PATH];
            // Récupère le chemin absolu de votre fichier .exe actuel
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            RegSetValueExW(hKey, L"GetMyWindowBack", 0, REG_SZ, (BYTE*)szPath, (DWORD)((wcslen(szPath) + 1) * sizeof(wchar_t)));
        }
        else {
            RegDeleteValueW(hKey, L"GetMyWindowBack");
        }
        RegCloseKey(hKey);
    }
}

// Procédure de gestion des messages de notre fenêtre cachée
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON: {
        // Clic Gauche : Liste les applications et génère le menu dynamique
        if (lParam == WM_LBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);

            // Re-scanner l'état des fenêtres du système à l'instant T
            g_detectedApps.clear();
            EnumWindows(EnumWindowsProc, 0);

            HMENU hMenu = CreatePopupMenu();

            // Remplir le menu avec les applications trouvées
            for (size_t i = 0; i < g_detectedApps.size(); ++i) {
                std::wstring menuText = L"[" + g_detectedApps[i].processName + L"] " + g_detectedApps[i].windowTitle;
                // Tronquer le texte si le titre est trop long pour le menu
                if (menuText.length() > 60) menuText = menuText.substr(0, 57) + L"...";

                AppendMenuW(hMenu, MF_STRING, ID_APP_START_RANGE + i, menuText.c_str());
            }

            if (g_detectedApps.empty()) {
                AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, L"(Aucune application d\u00E9tect\u00E9e)");
            }

            // Spécificité obligatoire de l'API Win32 pour les menus de la barre des tâches
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        // Clic Droit : Menu d'options
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
            std::wstring message =
                L"Sauveteur de fen\u00EAtres fant\u00F4mes\n\n"
                L"Cr\u00E9dits ic\u00F4ne :\n"
                L"\"Previous window\" Icon by Icojam on Icon-Icons.com\n\n"
                L"Voulez-vous visiter le site de l'auteur ?";

            int reponse = MessageBoxW(
                hwnd,
                message.c_str(),
                L"\u00C0 propos",
                MB_YESNO | MB_ICONINFORMATION | MB_SETFOREGROUND
            );

            // Si l'utilisateur clique sur "Oui", on lui ouvre le lien HTML
            if (reponse == IDYES) {
                ShellExecuteW(NULL, L"open", L"https://icon-icons.com/authors/59-icojam", NULL, NULL, SW_SHOWNORMAL);
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

// Point d'entrée de notre application d'arrière-plan sans console (WinMain)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Enregistrement d'une classe de fenêtre invisible chargée d'écouter les événements
    const wchar_t CLASS_NAME[] = L"TrayListenerWindowClass";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // Création de la fenêtre d'écoute (invisible)
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"TrayListener", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;

    // 2. Initialisation de l'icône dans la zone de notification
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;


    // On utilise une icône système par défaut (un point d'interrogation/information) pour éviter d'exiger un fichier .ico externe
    // g_nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    // LoadImage va chercher l'icône compilée dans le binaire (hInstance)
    g_nid.hIcon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_MY_TRAY_ICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), // S'adapte à la taille de la barre des tâches (16x16 ou 24x24)
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    );

    wcscpy_s(g_nid.szTip, L"Sauveteur de fen\u00EAtres fant\u00F4mes");

    Shell_NotifyIcon(NIM_ADD, &g_nid);

    // 3. Boucle de messages standard Win32
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
