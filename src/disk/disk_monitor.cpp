#include "linux2win/disk_monitor.hpp"
#include "linux2win/disk_enumerator.hpp"
#include <windows.h>
#include <dbt.h>
#include <initguid.h>
#include <usbiodef.h>
#include <iostream>

#pragma comment(lib, "user32.lib")

namespace linux2win {

// GUID for disk devices: {53f56307-b6bf-11d0-94f2-00a0c91efb8b}
DEFINE_GUID(GUID_DEVINTERFACE_DISK_DEVICE, 0x53f56307, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

DiskMonitor::DiskMonitor() = default;

DiskMonitor::~DiskMonitor() {
    stop();
}

bool DiskMonitor::start(DiskEventCallback callback) {
    if (running_) return true;

    callback_ = callback;
    running_ = true;

    worker_thread_ = std::thread(&DiskMonitor::monitor_thread_proc, this);
    return true;
}

void DiskMonitor::stop() {
    if (!running_) return;

    running_ = false;

    if (hwnd_) {
        PostMessageW(hwnd_, WM_QUIT, 0, 0);
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

LRESULT CALLBACK DiskMonitor::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    DiskMonitor* monitor = reinterpret_cast<DiskMonitor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (uMsg == WM_DEVICECHANGE && monitor && monitor->callback_) {
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
            if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                DiskEventType evType = (wParam == DBT_DEVICEARRIVAL) ? DiskEventType::Inserted : DiskEventType::Removed;

                // Rescan physical drives to determine which drive changed
                auto disks = DiskEnumerator::enumerate_all_disks();
                for (const auto& disk : disks) {
                    monitor->callback_(evType, disk);
                }
            }
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void DiskMonitor::monitor_thread_proc() {
    const wchar_t CLASS_NAME[] = L"Linux2WinDiskMonitorWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Linux2Win Disk Monitor Hidden Window",
        0, 0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd_) {
        running_ = false;
        return;
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Register device interface notification for disks
    DEV_BROADCAST_DEVICEINTERFACE_W dbi{};
    dbi.dbcc_size = sizeof(dbi);
    dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbi.dbcc_classguid = GUID_DEVINTERFACE_DISK_DEVICE;

    hDevNotify_ = RegisterDeviceNotificationW(
        hwnd_,
        &dbi,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    // Initial enumeration callback for currently present disks
    if (callback_) {
        auto disks = DiskEnumerator::enumerate_all_disks();
        for (const auto& disk : disks) {
            callback_(DiskEventType::Inserted, disk);
        }
    }

    // Message loop
    MSG msg;
    while (running_ && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hDevNotify_) {
        UnregisterDeviceNotification(hDevNotify_);
        hDevNotify_ = nullptr;
    }

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    UnregisterClassW(CLASS_NAME, GetModuleHandleW(nullptr));
}

} // namespace linux2win
