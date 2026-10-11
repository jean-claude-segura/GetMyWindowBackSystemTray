GetMyWindowBackSystemTray

Get your lost windows back with a single click.

GetMyWindowBackSystemTray is a lightweight Windows system tray utility designed to help you recover application windows that have ended up outside your visible desktop area, especially when switching between monitors or disconnecting an external display.

Instead of manually searching for misplaced windows or changing your display settings, simply click the application's tray icon, select the window you want to recover, and bring it back to your primary monitor.

✨ Features
Recover off-screen windows — Move application windows back to your primary monitor's work area.
Quick window selection — Left-click the system tray icon to display a list of detected windows.
Process identification — Display both the process name and window title to make it easier to identify the correct application.
Automatic window restoration — Restore minimized or maximized windows before repositioning them.
System tray integration — Run quietly in the background without opening a permanent application window or console.
Launch at Windows startup — Enable or disable automatic startup directly from the tray menu.
About dialog — Access project information and related links from the context menu.
Lightweight implementation — Built with native Windows APIs and C++.
🖥️ How to Use
1. Launch the application

Run GetMyWindowBackSystemTray.exe.

The application starts in the background and displays an icon in the Windows notification area (system tray).

2. Recover a window
Left-click the system tray icon.
Browse the list of detected application windows.
Select the window you want to recover.
The selected window is restored if minimized or maximized, moved to the top-left corner of the primary monitor's usable desktop area, and brought to the foreground when Windows permits it.

The menu displays entries in the following format:

[process.exe] Window Title

This helps distinguish between windows with similar titles.

3. Configure automatic startup
Right-click the system tray icon.
Select Launch at startup to enable or disable automatic startup.
When enabled, the application registers itself in the current user's Windows startup registry key.

This setting does not require modifying startup entries belonging to other applications.

4. Exit the application

Right-click the tray icon and select Quit to close the application and remove its notification area icon.

⚙️ How It Works

GetMyWindowBackSystemTray uses the native Windows API to enumerate and manipulate application windows.

When the tray icon is clicked, the application:

Enumerates top-level windows using EnumWindows.
Filters out invisible windows, owned windows, and windows without titles.
Retrieves window titles and, when permitted, their associated process names.
Builds a context menu containing the detected windows.
Restores and repositions the selected window using Windows window-management APIs.

The target window is moved to the top-left corner of the primary monitor's work area, which excludes reserved desktop space such as the taskbar.

The application runs a standard Win32 message loop and uses a hidden message-only window to receive system tray events.

📋 Requirements
Microsoft Windows.
A Windows C++ development environment to build from source.
The Windows SDK and standard Win32 development libraries.
Common Controls support (comctl32.lib), required for the SysLink controls used by the About dialog.

No additional third-party runtime libraries are explicitly required by the source code provided.

🔨 Building from Source

The project is written in C++ and uses the Windows API.

To build it:

Clone or download this repository.
Open the project in a compatible Windows C++ development environment, such as Microsoft Visual Studio.
Ensure that the Windows SDK is installed and configured.
Make sure the project includes the required source files, resource definitions, and icon resources.
Build the application using the Windows GUI subsystem so that no console window is created.

Repository: https://github.com/jean-claude-segura/GetMyWindowBackSystemTray

Important build details

The implementation uses:

windows.h for core Windows functionality.
shellapi.h for system tray integration and shell operations.
psapi.h for retrieving process names.
commctrl.h for Windows common controls.
resource.h and compiled resources for the tray icon and About dialog.
comctl32.lib for SysLink control support.

The source also uses MSVC-specific directives and functions, including #pragma comment, swprintf_s, and wcscpy_s. Microsoft Visual C++ is therefore the most straightforward compiler choice.

The exact build steps depend on the project files and resource definitions included in the repository.

🪟 Compatibility and Limitations
Primary monitor: Recovered windows are positioned on the primary monitor, regardless of which monitor they were previously associated with.
Window detection: Only visible, unowned top-level windows with non-empty titles are listed.
Process information: Process names may be unavailable when Windows denies access to a process or when that process has already terminated.
Foreground activation: Windows may prevent a window from becoming the foreground window due to its focus-stealing protection rules.
Startup configuration: The automatic startup option applies to the current Windows user.
Window placement: The selected window is placed at the top-left corner of the primary monitor's usable area. Its existing dimensions are preserved.

As with other Windows window-management utilities, behavior may vary depending on application permissions, window types, and Windows security restrictions.

🔒 Privacy

The application operates locally and uses Windows APIs to enumerate desktop windows and retrieve process names where access is permitted.

The provided implementation does not include network communication, telemetry, or remote data transmission.

📄 License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).

You are free to use, study, modify, and redistribute this software under the terms of the license. If you distribute modified versions, you must comply with the GPL-3.0 requirements, including making the corresponding source code available under the applicable terms.

See the LICENSE file for the full license text.

For more information, visit the GNU GPL v3.0 official page.

🤝 Contributing

Suggestions, bug reports, and improvements are welcome.

If you encounter a window that cannot be recovered, please consider reporting the relevant Windows version, monitor configuration, and application involved.

🔗 Repository

GetMyWindowBackSystemTray on GitHub

GetMyWindowBackSystemTray — Because no window should be lost forever.
