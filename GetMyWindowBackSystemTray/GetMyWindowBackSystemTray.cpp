#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <vector>
#include <string>
#include "resource.h"
#include <commctrl.h>

// Link the Common Controls library required by SysLink controls.
#pragma comment(lib, "comctl32.lib")

// Enable visual styles for Windows Common Controls version 6.
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Stores the information required to identify and restore a top-level application window.
struct AppWindow {
	HWND hwnd;
	std::wstring processName;
	std::wstring windowTitle;
};

// Shared state for the notification area icon and the windows discovered during enumeration.
NOTIFYICONDATA g_nid = { 0 };
std::vector<AppWindow> g_detectedApps;

// Handles initialization, hyperlink activation, and dismissal of the About dialog.
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG: {
		// Center the dialog within the primary monitor's work area,
		// excluding the taskbar and other reserved desktop regions.
		RECT rcDialog, rcScreen;

		// Obtain the dialog dimensions after Windows has created its controls.
		GetWindowRect(hDlg, &rcDialog);
		int width = rcDialog.right - rcDialog.left;
		int height = rcDialog.bottom - rcDialog.top;

		// Retrieve the usable dimensions of the primary monitor (excluding the taskbar).
		HMONITOR hPrimaryMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi = { sizeof(MONITORINFO) };

		if (GetMonitorInfoW(hPrimaryMonitor, &mi)) {
			rcScreen = mi.rcWork; // rcWork ensures the dialog does not overlap the taskbar.	
		}
		else {
			// Fall back to the primary screen dimensions if monitor information
			// cannot be retrieved. This fallback does not account for the taskbar.
			rcScreen.left = 0;
			rcScreen.top = 0;
			rcScreen.right = GetSystemMetrics(SM_CXSCREEN);
			rcScreen.bottom = GetSystemMetrics(SM_CYSCREEN);
		}

		int screenWidth = rcScreen.right - rcScreen.left;
		int screenHeight = rcScreen.bottom - rcScreen.top;

		int x = rcScreen.left + (screenWidth - width) / 2;
		int y = rcScreen.top + (screenHeight - height) / 2;

		// Preserve the dialog's dimensions while moving it to the calculated position.
		SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

		return (INT_PTR)TRUE;
	}

	case WM_NOTIFY: {
		// SysLink controls report hyperlink activation through WM_NOTIFY.
		LPNMHDR pNmhdr = (LPNMHDR)lParam;

		if (pNmhdr->code == NM_CLICK || pNmhdr->code == NM_RETURN) {
			PNMLINK pNmlink = (PNMLINK)lParam;

			// Delegate URL handling to the Windows shell and the user's default browser.
			ShellExecuteW(NULL, L"open", pNmlink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
			return (INT_PTR)TRUE;
		}
		break;
	}

	case WM_COMMAND:
		// Close the modal dialog when the default button or the Cancel command is invoked.
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}

	return (INT_PTR)FALSE;
}

// EnumWindows callback: collect visible, unowned top-level windows with non-empty titles.
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	if (!IsWindowVisible(hwnd)) return TRUE;

	// Exclude owned windows, which are generally dialogs or auxiliary windows
	// belonging to another top-level window.
	if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

	int titleLength = GetWindowTextLengthW(hwnd);
	if (titleLength == 0) return TRUE;

	// Retrieve the title using a buffer sized to the current reported text length.
	std::vector<wchar_t> titleBuffer(titleLength + 1);
	GetWindowTextW(hwnd, titleBuffer.data(), titleLength + 1);
	std::wstring windowTitle(titleBuffer.data());

	// Resolve the owning process so that similarly titled windows can be distinguished.
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);

	std::wstring processName = L"Inconnu";

	// Process inspection may fail because of access restrictions or process termination.
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

	if (hProcess != NULL) {
		wchar_t procNameBuffer[MAX_PATH];

		if (GetModuleBaseNameW(hProcess, NULL, procNameBuffer, MAX_PATH)) {
			processName = procNameBuffer;
		}

		CloseHandle(hProcess);
	}

	// Keep the HWND alongside its descriptive metadata for the tray menu command.
	g_detectedApps.push_back({ hwnd, processName, windowTitle });
	return TRUE;
}

// Restore a selected window and move it onto the primary monitor's work area.
void RapatrierSurEcranPrincipal(HWND targetHwnd) {
	HMONITOR hPrimaryMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi = { sizeof(MONITORINFO) };

	if (GetMonitorInfo(hPrimaryMonitor, &mi)) {
		RECT winRect;

		if (GetWindowRect(targetHwnd, &winRect)) {
			int width = winRect.right - winRect.left;
			int height = winRect.bottom - winRect.top;

			// Place the window at the top-left corner of the usable desktop area.
			int newX = mi.rcWork.left;
			int newY = mi.rcWork.top;

			// Restore minimized or maximized windows before applying the new position.
			if (IsZoomed(targetHwnd) || IsIconic(targetHwnd)) {
				ShowWindow(targetHwnd, SW_RESTORE);
			}

			SetWindowPos(targetHwnd, HWND_TOP, newX, newY, width, height, SWP_SHOWWINDOW);

			// Request foreground activation; Windows may restrict this operation
			// depending on the current input and foreground-lock state.
			SetForegroundWindow(targetHwnd);
		}
	}
}

// Check whether the application has a Run entry in the current user's registry hive.
bool EstAuDemarrage() {
	HKEY hKey;

	LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);

	if (lRes == ERROR_SUCCESS) {
		wchar_t path[MAX_PATH];
		DWORD size = sizeof(path);

		// The presence of the named value is treated as the enabled state.
		lRes = RegQueryValueExW(hKey, L"GetMyWindowBack", NULL, NULL, (LPBYTE)path, &size);

		RegCloseKey(hKey);
		return (lRes == ERROR_SUCCESS);
	}

	return false;
}

// Add or remove the per-user Run entry used to launch the application at sign-in.
void DefinirDemarrage(bool activer) {
	HKEY hKey;

	LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);

	if (lRes == ERROR_SUCCESS) {
		if (activer) {
			wchar_t szPath[MAX_PATH];

			// Register the executable's absolute path rather than a working-directory-relative path.
			GetModuleFileNameW(NULL, szPath, MAX_PATH);

			RegSetValueExW(hKey, L"GetMyWindowBack", 0, REG_SZ, (BYTE*)szPath, (DWORD)((wcslen(szPath) + 1) * sizeof(wchar_t)));
		}
		else {
			// Removing the value disables automatic launch without affecting other startup entries.
			RegDeleteValueW(hKey, L"GetMyWindowBack");
		}

		RegCloseKey(hKey);
	}
}

// Main window procedure for the hidden message-only window that receives tray callbacks.
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_TRAYICON: {
		if (lParam == WM_LBUTTONUP) {
			POINT curPoint;
			GetCursorPos(&curPoint);

			// Rebuild the list on each invocation so the menu reflects the current desktop state.
			g_detectedApps.clear();
			EnumWindows(EnumWindowsProc, 0);

			HMENU hMenu = CreatePopupMenu();

			for (size_t i = 0; i < g_detectedApps.size(); ++i) {
				// Include both process and window names to help identify the correct target.
				std::wstring menuText =
					L"[" + g_detectedApps[i].processName + L"] " +
					g_detectedApps[i].windowTitle;

				// Keep menu entries reasonably short to avoid excessively wide menus.
				if (menuText.length() > 60)
					menuText = menuText.substr(0, 57) + L"...";

				AppendMenuW(hMenu, MF_STRING, ID_APP_START_RANGE + i, menuText.c_str());
			}

			if (g_detectedApps.empty()) {
				AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, L"(Aucune application d\u00E9tect\u00E9e)");
			}

			// Give the owner window foreground status so the popup menu dismisses correctly.
			SetForegroundWindow(hwnd);

			TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);

			DestroyMenu(hMenu);
		}
		else if (lParam == WM_RBUTTONUP) {
			POINT curPoint;
			GetCursorPos(&curPoint);

			HMENU hMenu = CreatePopupMenu();

			// Reflect the current startup configuration in the menu's check mark.
			UINT flags = MF_STRING;

			if (EstAuDemarrage())
				flags |= MF_CHECKED;

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
		// Menu command identifiers are stored in the low-order word of wParam.
		int wmId = LOWORD(wParam);

		if (wmId == ID_TRAY_EXIT) {
			// Remove the notification icon before terminating the message loop.
			Shell_NotifyIcon(NIM_DELETE, &g_nid);
			PostQuitMessage(0);
		}
		else if (wmId == ID_TRAY_STARTUP) {
			// Toggle the startup entry based on its current registry state.
			bool etatActuel = EstAuDemarrage();
			DefinirDemarrage(!etatActuel);
		}
		else if (wmId == ID_TRAY_ABOUT) {
			// Open the modal About dialog using the application's compiled resources.
			const auto result = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT_DIALOG), hwnd, AboutDlgProc);

			// DialogBoxW returns -1 if creation fails; inspect the Win32 error for diagnostics.
			if (result == -1) {
				DWORD error = GetLastError();
				wchar_t diagnostic[256];

				swprintf_s(
					diagnostic,
					L"Erreur Win32 fatale !\n\n"
					L"La bo\u00EEte de dialogue n'a pas pu s'ouvrir.\n"
					L"Code d'erreur renvoy\u00E9 par Windows : %lu",
					error
				);

				MessageBoxW(hwnd, diagnostic, L"Diagnostic Sauveteur", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
			}
		}
		else if (
			wmId >= ID_APP_START_RANGE &&
			wmId < ID_APP_START_RANGE + (int)g_detectedApps.size()
			) {
			// Translate the menu command back into the corresponding window-list index.
			int index = wmId - ID_APP_START_RANGE;

			// Restore and reposition the window selected by the user.
			RapatrierSurEcranPrincipal(g_detectedApps[index].hwnd);
		}
		break;
	}

	case WM_DESTROY:
		// Ensure the notification icon is removed if the hidden window is destroyed.
		Shell_NotifyIcon(NIM_DELETE, &g_nid);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

// Application entry point for the GUI subsystem; no console window is created.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// Register the SysLink common-control class before loading a dialog that uses it.
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LINK_CLASS;
	InitCommonControlsEx(&icex);

	// Register a dedicated window class for receiving notification-area messages.
	const wchar_t CLASS_NAME[] = L"TrayListenerWindowClass";

	WNDCLASSW wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClassW(&wc);

	// A message-only window receives messages without creating a visible top-level window.
	HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"TrayListener", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

	if (hwnd == NULL) return 0;

	// Configure the notification icon and its callback message.
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hwnd;
	g_nid.uID = 1;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;

	// Load the tray-sized icon from the executable's resources.
	g_nid.hIcon = (HICON)LoadImage(
		hInstance,
		MAKEINTRESOURCE(IDI_MY_TRAY_ICON),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR
	);

	// Set the tooltip displayed when the user hovers over the notification icon.
	wcscpy_s(g_nid.szTip, L"Sauveteur de fen\u00EAtres fant\u00F4mes");

	// Register the icon with the Windows notification area.
	Shell_NotifyIcon(NIM_ADD, &g_nid);

	// Standard Win32 message loop for dispatching tray callbacks and window commands.
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
