#include "linux2win/tray_icon.hpp"
#include <cstring>
#include <iostream>

namespace linux2win {

constexpr UINT IDM_OPEN_EXPLORER    = 1001;
constexpr UINT IDM_RESCAN_DISKS     = 1002;
constexpr UINT IDM_TOGGLE_STARTUP   = 1003;
constexpr UINT IDM_ABOUT            = 1004;
constexpr UINT IDM_EXIT             = 1005;

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    remove();
}

bool TrayIcon::init(HINSTANCE hInstance, HWND hwndParent, UINT uCallbackMessage, const std::wstring& tooltip) {
    if (is_visible_) return true;

    hwnd_ = hwndParent;

    std::memset(&nid_, 0, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwndParent;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = uCallbackMessage;
    nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION); // Standard clean drive/app icon

    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);

    is_visible_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    return is_visible_;
}

void TrayIcon::remove() {
    if (is_visible_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        is_visible_ = false;
    }
}

void TrayIcon::show_notification(const std::wstring& title, const std::wstring& message, DWORD infoFlags) {
    if (!is_visible_) return;

    nid_.uFlags |= NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, message.c_str(), _TRUNCATE);
    nid_.dwInfoFlags = infoFlags;
    nid_.uTimeout = 5000;

    Shell_NotifyIconW(NIM_MODIFY, &nid_);

    // Remove NIF_INFO flag so subsequent updates don't keep re-triggering the same balloon
    nid_.uFlags &= ~NIF_INFO;
}

void TrayIcon::show_context_menu(HWND hwnd, bool startup_enabled,
                                 std::function<void()> on_open_explorer,
                                 std::function<void()> on_rescan,
                                 std::function<void(bool)> on_toggle_startup,
                                 std::function<void()> on_about,
                                 std::function<void()> on_exit) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_OPEN_EXPLORER, L"📂 Open Linux Disks (C:\\LinuxDisks)");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_RESCAN_DISKS, L"🔍 Rescan Disks Now");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    UINT startupFlags = MF_BYPOSITION | MF_STRING | (startup_enabled ? MF_CHECKED : MF_UNCHECKED);
    InsertMenuW(hMenu, -1, startupFlags, IDM_TOGGLE_STARTUP, L"✔️ Run on Windows Startup (Memory-Resident)");

    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT, L"ℹ️ About & Read-Only Safety Info");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"❌ Exit Linux2Win");

    // Set default bold item to "Open Linux Disks"
    SetMenuDefaultItem(hMenu, IDM_OPEN_EXPLORER, FALSE);

    POINT pt;
    GetCursorPos(&pt);

    // Required by Windows before calling TrackPopupMenu from a tray icon
    SetForegroundWindow(hwnd);

    UINT cmd = TrackPopupMenu(
        hMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        pt.x, pt.y,
        0,
        hwnd,
        nullptr
    );

    DestroyMenu(hMenu);

    switch (cmd) {
        case IDM_OPEN_EXPLORER:
            if (on_open_explorer) on_open_explorer();
            break;
        case IDM_RESCAN_DISKS:
            if (on_rescan) on_rescan();
            break;
        case IDM_TOGGLE_STARTUP:
            if (on_toggle_startup) on_toggle_startup(!startup_enabled);
            break;
        case IDM_ABOUT:
            if (on_about) on_about();
            break;
        case IDM_EXIT:
            if (on_exit) on_exit();
            break;
        default:
            break;
    }
}

} // namespace linux2win
