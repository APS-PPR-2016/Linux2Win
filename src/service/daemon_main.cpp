#include "linux2win/disk_enumerator.hpp"
#include "linux2win/disk_monitor.hpp"
#include "linux2win/partition_filter.hpp"
#include "linux2win/block_device.hpp"
#include "linux2win/ext4_reader.hpp"
#include "linux2win/vfs_tree.hpp"
#include "linux2win/projfs_mount.hpp"
#include "linux2win/startup_manager.hpp"
#include "linux2win/tray_icon.hpp"

#include <windows.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

using namespace linux2win;

constexpr UINT WM_TRAYICON_MSG = WM_USER + 100;

struct ActiveMount {
    PartitionInfo part;
    std::wstring mount_dir;
    std::shared_ptr<BlockDevice> device;
    std::shared_ptr<fs::Ext4Reader> reader;
    std::shared_ptr<vfs::VfsTree> tree;
    std::shared_ptr<vfs::ProjFSMount> projfs;
};

static std::map<std::pair<uint32_t, uint32_t>, ActiveMount> g_active_mounts;
static std::mutex g_mount_mutex;
static TrayIcon g_tray_icon;
static std::unique_ptr<DiskMonitor> g_disk_monitor;
static HWND g_hwnd = nullptr;

static std::wstring get_mount_dir_for_partition(const PartitionInfo& part) {
    std::string dir = "C:\\LinuxDisks\\Disk" + std::to_string(part.disk_index) + "_Part" + std::to_string(part.partition_number);
    if (!part.fs_label.empty()) {
        dir += "_" + part.fs_label;
    }
    return std::wstring(dir.begin(), dir.end());
}

static bool mount_data_partition(const PartitionInfo& part) {
    auto key = std::make_pair(part.disk_index, part.partition_number);

    std::lock_guard<std::mutex> lock(g_mount_mutex);
    if (g_active_mounts.find(key) != g_active_mounts.end()) {
        return true; // Already mounted
    }

    std::wstring mount_dir = get_mount_dir_for_partition(part);
    std::string disk_path = "\\\\.\\PhysicalDrive" + std::to_string(part.disk_index);

    auto dev = std::make_shared<BlockDevice>();
    if (!dev->open_read_only(disk_path, part.starting_offset, part.total_length)) {
        return false;
    }

    auto reader = std::make_shared<fs::Ext4Reader>();
    if (!reader->mount(dev)) {
        return false;
    }

    auto tree = std::make_shared<vfs::VfsTree>(reader);
    auto projfs = std::make_shared<vfs::ProjFSMount>(tree, mount_dir);

    bool projfs_started = projfs->start();

    ActiveMount mount;
    mount.part = part;
    mount.mount_dir = mount_dir;
    mount.device = dev;
    mount.reader = reader;
    mount.tree = tree;
    mount.projfs = projfs;

    g_active_mounts[key] = std::move(mount);

    // Show balloon notification in Windows notification center
    std::wstring msg = L"Mounted Linux Data Partition (Disk " + std::to_wstring(part.disk_index) + 
                       L", Part " + std::to_wstring(part.partition_number) + L") to " + mount_dir +
                       L"\nRead-Only safety protection active.";
    g_tray_icon.show_notification(L"Linux Disk Detected & Mounted", msg);

    return true;
}

static void unmount_data_partition(uint32_t disk_index, uint32_t partition_number) {
    auto key = std::make_pair(disk_index, partition_number);

    std::lock_guard<std::mutex> lock(g_mount_mutex);
    auto it = g_active_mounts.find(key);
    if (it != g_active_mounts.end()) {
        if (it->second.projfs) it->second.projfs->stop();
        if (it->second.reader) it->second.reader->unmount();
        if (it->second.device) it->second.device->close();

        std::wstring msg = L"Linux disk partition (Disk " + std::to_wstring(disk_index) + 
                           L", Part " + std::to_wstring(partition_number) + L") was safely unmounted.";
        g_tray_icon.show_notification(L"Linux Disk Removed", msg);

        g_active_mounts.erase(it);
    }
}

static void unmount_all_partitions_for_disk(uint32_t disk_index) {
    std::vector<std::pair<uint32_t, uint32_t>> to_remove;
    {
        std::lock_guard<std::mutex> lock(g_mount_mutex);
        for (const auto& [k, v] : g_active_mounts) {
            if (k.first == disk_index) {
                to_remove.push_back(k);
            }
        }
    }
    for (const auto& k : to_remove) {
        unmount_data_partition(k.first, k.second);
    }
}

static void rescan_and_mount_all() {
    auto mountable = DiskEnumerator::get_all_mountable_partitions();
    for (const auto& part : mountable) {
        mount_data_partition(part);
    }
}

static LRESULT CALLBACK DaemonWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TRAYICON_MSG) {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            bool startup_on = StartupManager::is_startup_enabled();
            g_tray_icon.show_context_menu(
                hwnd,
                startup_on,
                []() { // Open in Explorer
                    StartupManager::open_in_explorer(L"C:\\LinuxDisks");
                },
                []() { // Rescan Now
                    rescan_and_mount_all();
                    g_tray_icon.show_notification(L"Linux2Win", L"Disk scan complete. Linux partitions updated.");
                },
                [](bool new_state) { // Toggle Startup
                    std::wstring exe = StartupManager::get_current_executable_path();
                    if (new_state) {
                        StartupManager::enable_startup(exe);
                        g_tray_icon.show_notification(L"Startup Enabled", L"Linux2Win will load automatically on Windows boot.");
                    } else {
                        StartupManager::disable_startup();
                        g_tray_icon.show_notification(L"Startup Disabled", L"Linux2Win will not load automatically on boot.");
                    }
                },
                []() { // About
                    MessageBoxW(
                        nullptr,
                        L"Linux2Win Memory-Resident Daemon v1.0.0\n\n"
                        L"• Automatic hotplug detection for Linux disks & USBs\n"
                        L"• Native Windows Explorer integration (C:\\LinuxDisks)\n"
                        L"• Ext2/Ext3/Ext4 partition reading with ProjFS\n"
                        L"• 100% Read-Only Safety Protection\n"
                        L"• Boot, EFI, and Swap partitions excluded",
                        L"About Linux2Win",
                        MB_OK | MB_ICONINFORMATION
                    );
                },
                [hwnd]() { // Exit
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            );
            return 0;
        }
    } else if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
        // Unmount all active partitions
        {
            std::lock_guard<std::mutex> lock(g_mount_mutex);
            for (auto& [k, v] : g_active_mounts) {
                if (v.projfs) v.projfs->stop();
                if (v.reader) v.reader->unmount();
                if (v.device) v.device->close();
            }
            g_active_mounts.clear();
        }

        g_tray_icon.remove();
        if (g_disk_monitor) g_disk_monitor->stop();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Check if another instance is already running
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\Linux2Win_Daemon_Instance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Linux2Win Memory-Resident Daemon is already running in the System Tray.", L"Linux2Win", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Ensure mount directory and shortcuts exist
    StartupManager::ensure_mount_root_exists(L"C:\\LinuxDisks");
    StartupManager::create_explorer_shortcuts(L"C:\\LinuxDisks");

    // Check if permission prompt is needed
    std::wstring cmdline(pCmdLine ? pCmdLine : L"");
    bool quiet_mode = (cmdline.find(L"--quiet") != std::wstring::npos || cmdline.find(L"--daemon") != std::wstring::npos);

    if (!quiet_mode) {
        StartupManager::prompt_user_permission(false);
    }

    // Register message window class
    const wchar_t CLASS_NAME[] = L"Linux2WinDaemonWindowClass";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DaemonWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Linux2Win Daemon Window",
        0, 0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_hwnd) return 1;

    // Initialize tray icon
    g_tray_icon.init(hInstance, g_hwnd, WM_TRAYICON_MSG, L"Linux2Win - Linux Disk Auto-Mount VFS (Active)");

    // Start background hotplug monitor
    g_disk_monitor = std::make_unique<DiskMonitor>();
    g_disk_monitor->start([](DiskEventType type, const DiskInfo& disk) {
        if (type == DiskEventType::Inserted) {
            for (const auto& part : disk.partitions) {
                if (part.is_data_partition && !PartitionFilter::is_excluded_system_partition(part.kind)) {
                    mount_data_partition(part);
                }
            }
        } else {
            unmount_all_partitions_for_disk(disk.disk_index);
        }
    });

    // Initial mount for already connected Linux disks
    rescan_and_mount_all();

    // Show initial welcome balloon
    g_tray_icon.show_notification(
        L"Linux2Win Active",
        L"Linux2Win is monitoring for Linux disks. Access mounted drives in C:\\LinuxDisks or Desktop shortcut."
    );

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
