#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

namespace linux2win {

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool init(HINSTANCE hInstance, HWND hwndParent, UINT uCallbackMessage, const std::wstring& tooltip = L"Linux2Win - Linux Disk Monitor");
    void remove();

    // Show balloon notification in Windows notification center
    void show_notification(const std::wstring& title, const std::wstring& message, DWORD infoFlags = NIIF_INFO);

    // Show context menu at cursor position
    void show_context_menu(HWND hwnd, bool startup_enabled,
                           std::function<void()> on_open_explorer,
                           std::function<void()> on_rescan,
                           std::function<void(bool)> on_toggle_startup,
                           std::function<void()> on_about,
                           std::function<void()> on_exit);

private:
    NOTIFYICONDATAW nid_{};
    bool is_visible_{false};
    HWND hwnd_{nullptr};
};

} // namespace linux2win
