#include "linux2win/startup_manager.hpp"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <iostream>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

namespace linux2win {

static const wchar_t TASK_NAME[] = L"Linux2Win Resident Daemon";
static const wchar_t REG_KEY_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t REG_VALUE_NAME[] = L"Linux2Win";
static const wchar_t CONFIG_KEY_PATH[] = L"Software\\Linux2Win";

std::wstring StartupManager::get_current_executable_path() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::wstring(path);
}

bool StartupManager::is_startup_enabled() {
    // Check registry first
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[MAX_PATH] = {0};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, REG_VALUE_NAME, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS && buffer[0] != L'\0') {
            return true;
        }
    }

    // Check Task Scheduler via registry setting or schtasks query
    if (RegOpenKeyExW(HKEY_CURRENT_USER, CONFIG_KEY_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD enabled = 0;
        DWORD size = sizeof(enabled);
        LONG res = RegQueryValueExW(hKey, L"StartupEnabled", nullptr, nullptr, reinterpret_cast<LPBYTE>(&enabled), &size);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS && enabled == 1) {
            return true;
        }
    }

    return false;
}

PermissionResult StartupManager::prompt_user_permission(bool force_prompt) {
    if (!force_prompt) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, CONFIG_KEY_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD configured = 0;
            DWORD size = sizeof(configured);
            LONG res = RegQueryValueExW(hKey, L"PermissionPromptShown", nullptr, nullptr, reinterpret_cast<LPBYTE>(&configured), &size);
            RegCloseKey(hKey);
            if (res == ERROR_SUCCESS && configured == 1) {
                return PermissionResult::AlreadyConfigured;
            }
        }
    }

    // Prepare informative dialog asking the user for memory-resident permission
    const wchar_t title[] = L"Linux2Win - Background Monitor & Explorer Integration";
    const wchar_t message[] = 
        L"Would you like Linux2Win to run in the background on Windows startup?\n\n"
        L"Benefits of enabling Memory-Resident Mode:\n"
        L" • Automatically detects inserted Linux USB drives & disks in real time\n"
        L" • Automatically displays Linux data partitions inside Windows File Explorer\n"
        L" • Enables standard Windows operations (browsing, opening, dragging & copying files)\n"
        L" • 100% Read-Only Safety Protection (prevents accidental writes or corruption)\n"
        L" • Excludes boot, EFI, and swap partitions automatically\n\n"
        L"Click 'Yes' to enable automatic background monitoring on startup,\n"
        L"or 'No' to run Linux2Win manually only when needed.";

    int response = MessageBoxW(
        nullptr,
        message,
        title,
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST
    );

    // Save user choice in registry
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, CONFIG_KEY_PATH, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD shown = 1;
        RegSetValueExW(hKey, L"PermissionPromptShown", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&shown), sizeof(shown));
        DWORD enabled = (response == IDYES) ? 1 : 0;
        RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
        RegCloseKey(hKey);
    }

    if (response == IDYES) {
        std::wstring exe_path = get_current_executable_path();
        // If current is installer, replace with daemon path
        enable_startup(exe_path);
        create_explorer_shortcuts();
        return PermissionResult::Granted;
    } else {
        disable_startup();
        return PermissionResult::Denied;
    }
}

bool StartupManager::enable_startup(const std::wstring& executable_path) {
    // 1. Create task in Windows Task Scheduler with HighestAvailable privileges
    // so physical disk read permissions are granted without prompt on logon
    std::wstring cmd = L"schtasks /create /tn \"" + std::wstring(TASK_NAME) +
                       L"\" /tr \"\\\"" + executable_path + L"\\\" --daemon\" /sc onlogon /rl highest /f";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(L'\0');

    bool task_created = false;
    if (CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        task_created = (exitCode == 0);
    }

    // 2. Also register in HKCU Run key for fallback
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        std::wstring run_cmd = L"\"" + executable_path + L"\" --daemon";
        RegSetValueExW(hKey, REG_VALUE_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*>(run_cmd.c_str()), static_cast<DWORD>((run_cmd.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // Update config key
    if (RegCreateKeyExW(HKEY_CURRENT_USER, CONFIG_KEY_PATH, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD enabled = 1;
        RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
        RegCloseKey(hKey);
    }

    return true;
}

bool StartupManager::disable_startup() {
    // 1. Remove task scheduler task
    std::wstring cmd = L"schtasks /delete /tn \"" + std::wstring(TASK_NAME) + L"\" /f";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // 2. Remove registry run entry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, REG_VALUE_NAME);
        RegCloseKey(hKey);
    }

    // Update config key
    if (RegCreateKeyExW(HKEY_CURRENT_USER, CONFIG_KEY_PATH, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD enabled = 0;
        RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
        RegCloseKey(hKey);
    }

    return true;
}

static bool create_shell_shortcut(const std::wstring& target_path, const std::wstring& shortcut_path, const std::wstring& description) {
    CoInitialize(nullptr);
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(target_path.c_str());
        psl->SetDescription(description.c_str());

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcut_path.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

bool StartupManager::ensure_mount_root_exists(const std::wstring& path) {
    std::filesystem::path p(path);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) {
        return std::filesystem::create_directories(p, ec);
    }
    return true;
}

bool StartupManager::create_explorer_shortcuts(const std::wstring& target_mount_root) {
    ensure_mount_root_exists(target_mount_root);

    wchar_t desktop_path[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop_path) == S_OK) {
        std::wstring link_path = std::wstring(desktop_path) + L"\\Linux Disks.lnk";
        create_shell_shortcut(target_mount_root, link_path, L"Access connected Linux data partitions in Windows Explorer");
    }

    wchar_t start_menu_path[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, start_menu_path) == S_OK) {
        std::wstring link_path = std::wstring(start_menu_path) + L"\\Linux Disks.lnk";
        create_shell_shortcut(target_mount_root, link_path, L"Access connected Linux data partitions in Windows Explorer");
    }

    return true;
}

bool StartupManager::remove_explorer_shortcuts() {
    wchar_t desktop_path[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop_path) == S_OK) {
        std::wstring link_path = std::wstring(desktop_path) + L"\\Linux Disks.lnk";
        std::filesystem::remove(link_path);
    }

    wchar_t start_menu_path[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, start_menu_path) == S_OK) {
        std::wstring link_path = std::wstring(start_menu_path) + L"\\Linux Disks.lnk";
        std::filesystem::remove(link_path);
    }

    return true;
}

bool StartupManager::open_in_explorer(const std::wstring& path) {
    ensure_mount_root_exists(path);
    HINSTANCE hInst = ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(hInst) > 32;
}

} // namespace linux2win
