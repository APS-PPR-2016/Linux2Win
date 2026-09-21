#pragma once

#include <string>
#include <vector>

namespace linux2win {

enum class PermissionResult {
    Granted,
    Denied,
    AlreadyConfigured
};

class StartupManager {
public:
    // Check if auto-start is currently enabled in Task Scheduler / Registry
    static bool is_startup_enabled();

    // Prompts the user with a native Win32 interactive dialog asking permission
    // to run Linux2Win as a memory-resident background daemon on Windows startup.
    static PermissionResult prompt_user_permission(bool force_prompt = false);

    // Enables auto-start on Windows startup (uses Task Scheduler with elevated privileges for raw disk access)
    static bool enable_startup(const std::wstring& executable_path);

    // Disables auto-start
    static bool disable_startup();

    // Creates Explorer shortcuts (Desktop, Start Menu, Quick Access folder link)
    static bool create_explorer_shortcuts(const std::wstring& target_mount_root = L"C:\\LinuxDisks");

    // Removes Explorer shortcuts
    static bool remove_explorer_shortcuts();

    // Ensures target mount directory exists with proper permissions
    static bool ensure_mount_root_exists(const std::wstring& path = L"C:\\LinuxDisks");

    // Open target folder in Windows File Explorer
    static bool open_in_explorer(const std::wstring& path = L"C:\\LinuxDisks");

    // Helper: get current running executable path
    static std::wstring get_current_executable_path();
};

} // namespace linux2win
