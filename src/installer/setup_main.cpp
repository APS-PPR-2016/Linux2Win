#include "linux2win/startup_manager.hpp"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

constexpr UINT_PTR IDC_CHK_STARTUP   = 101;
constexpr UINT_PTR IDC_CHK_SHORTCUTS = 102;
constexpr UINT_PTR IDC_CHK_PROJFS    = 103;
constexpr UINT_PTR IDC_CHK_LAUNCH    = 104;
constexpr UINT_PTR IDC_EDIT_PATH     = 105;
constexpr UINT_PTR IDC_BTN_BROWSE    = 106;
constexpr UINT_PTR IDC_BTN_INSTALL   = 107;
constexpr UINT_PTR IDC_BTN_CANCEL    = 108;

static HWND g_hEditPath = nullptr;
static HWND g_hChkStartup = nullptr;
static HWND g_hChkShortcuts = nullptr;
static HWND g_hChkProjFS = nullptr;
static HWND g_hChkLaunch = nullptr;
static HWND g_hBtnInstall = nullptr;

static std::wstring g_install_dir = L"C:\\Program Files\\Linux2Win";

static void enable_projfs_feature() {
    // Run dism command to enable client ProjFS feature quietly
    std::wstring cmd = L"dism /online /enable-feature /featurename:Client-ProjFS /norestart";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static bool perform_installation(HWND hwndParent, const std::wstring& target_dir,
                                 bool enable_startup_opt, bool create_shortcuts_opt,
                                 bool enable_projfs_opt, bool launch_now_opt) {
    try {
        fs::path target_path(target_dir);
        fs::create_directories(target_path);

        // Get directory of installer executable
        wchar_t current_exe[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, current_exe, MAX_PATH);
        fs::path source_dir = fs::path(current_exe).parent_path();

        // Copy files
        std::vector<std::wstring> files_to_copy = {
            L"linux2win.exe",
            L"linux2win_daemon.exe",
            L"README.md"
        };

        for (const auto& file : files_to_copy) {
            fs::path src = source_dir / file;
            fs::path dst = target_path / file;
            if (fs::exists(src)) {
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
            }
        }

        // Copy self as uninstaller
        fs::path uninstaller_dst = target_path / L"uninstall.exe";
        fs::copy_file(current_exe, uninstaller_dst, fs::copy_options::overwrite_existing);

        // Enable ProjFS if requested
        if (enable_projfs_opt) {
            enable_projfs_feature();
        }

        // Setup shortcuts
        if (create_shortcuts_opt) {
            linux2win::StartupManager::create_explorer_shortcuts(L"C:\\LinuxDisks");

            // Also create App shortcut in Start Menu
            wchar_t start_menu[MAX_PATH] = {0};
            if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, start_menu) == S_OK) {
                fs::path app_lnk = fs::path(start_menu) / L"Linux2Win Daemon.lnk";
                std::wstring daemon_exe = (target_path / L"linux2win_daemon.exe").wstring();
                
                // Write shortcut
                CoInitialize(nullptr);
                IShellLinkW* psl = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl)))) {
                    psl->SetPath(daemon_exe.c_str());
                    psl->SetDescription(L"Linux2Win Linux Disk Auto-Mount VFS");
                    IPersistFile* ppf = nullptr;
                    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf)))) {
                        ppf->Save(app_lnk.c_str(), TRUE);
                        ppf->Release();
                    }
                    psl->Release();
                }
            }
        }

        // Setup startup memory-resident task
        std::wstring daemon_path = (target_path / L"linux2win_daemon.exe").wstring();
        if (enable_startup_opt) {
            linux2win::StartupManager::enable_startup(daemon_path);
        } else {
            linux2win::StartupManager::disable_startup();
        }

        // Register in Add/Remove Programs
        HKEY hKey;
        const wchar_t UNINSTALL_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Linux2Win";
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            std::wstring dispName = L"Linux2Win (Linux Disk Auto-Detector & Windows Folder VFS)";
            std::wstring uninstCmd = L"\"" + uninstaller_dst.wstring() + L"\" /uninstall";
            std::wstring version = L"1.0.0";
            std::wstring publisher = L"Linux2Win";

            RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, reinterpret_cast<const BYTE*>(dispName.c_str()), static_cast<DWORD>((dispName.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, reinterpret_cast<const BYTE*>(uninstCmd.c_str()), static_cast<DWORD>((uninstCmd.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, reinterpret_cast<const BYTE*>(version.c_str()), static_cast<DWORD>((version.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, reinterpret_cast<const BYTE*>(publisher.c_str()), static_cast<DWORD>((publisher.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, reinterpret_cast<const BYTE*>(target_dir.c_str()), static_cast<DWORD>((target_dir.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }

        // Launch now if requested
        if (launch_now_opt) {
            ShellExecuteW(nullptr, L"open", daemon_path.c_str(), L"--daemon", nullptr, SW_SHOWNORMAL);
        }

        MessageBoxW(
            hwndParent,
            L"Linux2Win has been successfully installed!\n\n"
            L"• Connected Linux data partitions will automatically appear in C:\\LinuxDisks and Windows Explorer.\n"
            L"• Read-only safety protection is active.\n"
            L"• System Tray icon is ready.",
            L"Installation Complete",
            MB_OK | MB_ICONINFORMATION
        );

        return true;
    } catch (const std::exception& ex) {
        std::string err = ex.what();
        std::wstring werr(err.begin(), err.end());
        MessageBoxW(hwndParent, (L"Installation failed: " + werr).c_str(), L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
}

static void perform_uninstallation(HWND hwndParent) {
    int res = MessageBoxW(
        hwndParent,
        L"Are you sure you want to uninstall Linux2Win?\n\nThis will remove the background monitor, startup tasks, and shortcuts.",
        L"Uninstall Linux2Win",
        MB_YESNO | MB_ICONQUESTION
    );

    if (res != IDYES) return;

    // 1. Stop daemon if running
    HWND hDaemon = FindWindowW(L"Linux2WinDaemonWindowClass", nullptr);
    if (hDaemon) {
        PostMessageW(hDaemon, WM_CLOSE, 0, 0);
        Sleep(1000);
    }

    // 2. Remove startup tasks & shortcuts
    linux2win::StartupManager::disable_startup();
    linux2win::StartupManager::remove_explorer_shortcuts();

    // 3. Remove Start Menu app shortcut
    wchar_t start_menu[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, start_menu) == S_OK) {
        fs::path app_lnk = fs::path(start_menu) / L"Linux2Win Daemon.lnk";
        fs::remove(app_lnk);
    }

    // 4. Remove Add/Remove Programs registry key
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Linux2Win");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Linux2Win");

    MessageBoxW(
        hwndParent,
        L"Linux2Win has been successfully uninstalled from your system.",
        L"Uninstall Complete",
        MB_OK | MB_ICONINFORMATION
    );
}

static LRESULT CALLBACK InstallerWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            // Title Banner
            HWND hTitle = CreateWindowExW(0, L"STATIC", L"Install Linux2Win",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                25, 20, 480, 30, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDesc = CreateWindowExW(0, L"STATIC",
                L"Linux Disk Auto-Detector & Windows Folder VFS with Read-Only Safety Protection.\n"
                L"Easily browse, copy files, and manage Linux ext2/ext3/ext4 disks directly inside Windows Explorer.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                25, 55, 480, 40, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Path label & Edit
            HWND hLblPath = CreateWindowExW(0, L"STATIC", L"Installation Folder:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                25, 110, 200, 20, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblPath, WM_SETFONT, (WPARAM)hFont, TRUE);

            g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_install_dir.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                25, 130, 380, 24, hwnd, reinterpret_cast<HMENU>(IDC_EDIT_PATH), nullptr, nullptr);
            SendMessageW(g_hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                415, 130, 90, 24, hwnd, reinterpret_cast<HMENU>(IDC_BTN_BROWSE), nullptr, nullptr);
            SendMessageW(hBtnBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Permission & Configuration Checkboxes
            g_hChkStartup = CreateWindowExW(0, L"BUTTON",
                L"Load on Windows startup as a Memory-Resident monitor (Recommended)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                25, 175, 480, 22, hwnd, reinterpret_cast<HMENU>(IDC_CHK_STARTUP), nullptr, nullptr);
            SendMessageW(g_hChkStartup, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hChkStartup, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkShortcuts = CreateWindowExW(0, L"BUTTON",
                L"Create Windows Explorer & Desktop shortcuts to C:\\LinuxDisks",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                25, 205, 480, 22, hwnd, reinterpret_cast<HMENU>(IDC_CHK_SHORTCUTS), nullptr, nullptr);
            SendMessageW(g_hChkShortcuts, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hChkShortcuts, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkProjFS = CreateWindowExW(0, L"BUTTON",
                L"Enable Windows Projected File System (ProjFS) for native Explorer folder view",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                25, 235, 480, 22, hwnd, reinterpret_cast<HMENU>(IDC_CHK_PROJFS), nullptr, nullptr);
            SendMessageW(g_hChkProjFS, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hChkProjFS, BM_SETCHECK, BST_CHECKED, 0);

            g_hChkLaunch = CreateWindowExW(0, L"BUTTON",
                L"Launch Linux2Win background service immediately after installation",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                25, 265, 480, 22, hwnd, reinterpret_cast<HMENU>(IDC_CHK_LAUNCH), nullptr, nullptr);
            SendMessageW(g_hChkLaunch, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);

            // Install & Cancel Buttons
            g_hBtnInstall = CreateWindowExW(0, L"BUTTON", L"Install Now",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                300, 315, 100, 30, hwnd, reinterpret_cast<HMENU>(IDC_BTN_INSTALL), nullptr, nullptr);
            SendMessageW(g_hBtnInstall, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                410, 315, 95, 30, hwnd, reinterpret_cast<HMENU>(IDC_BTN_CANCEL), nullptr, nullptr);
            SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BTN_INSTALL) {
                wchar_t pathBuf[MAX_PATH] = {0};
                GetWindowTextW(g_hEditPath, pathBuf, MAX_PATH);

                bool startup = (SendMessageW(g_hChkStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool shortcuts = (SendMessageW(g_hChkShortcuts, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool projfs = (SendMessageW(g_hChkProjFS, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool launch = (SendMessageW(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);

                if (perform_installation(hwnd, pathBuf, startup, shortcuts, projfs, launch)) {
                    DestroyWindow(hwnd);
                }
            } else if (wmId == IDC_BTN_CANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    std::wstring cmd(pCmdLine ? pCmdLine : L"");

    if (cmd.find(L"/uninstall") != std::wstring::npos || cmd.find(L"--uninstall") != std::wstring::npos) {
        perform_uninstallation(nullptr);
        return 0;
    }

    InitCommonControls();

    const wchar_t CLASS_NAME[] = L"Linux2WinInstallerWindowClass";
    WNDCLASSW wc{};
    wc.lpfnWndProc = InstallerWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int winWidth = 545;
    int winHeight = 405;
    int posX = (screenWidth - winWidth) / 2;
    int posY = (screenHeight - winHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"Linux2Win Setup - Windows Installer & Startup Configuration",
        WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        posX, posY, winWidth, winHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
