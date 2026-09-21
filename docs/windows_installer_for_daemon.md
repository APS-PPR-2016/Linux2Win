# Windows Installer & Memory-Resident Daemon for Linux2Win

Implementation plan to create a dedicated **Windows Installer**, **Background Memory-Resident Daemon** (with System Tray notifications and Explorer links), and an **Interactive User Permission Prompt** to run on Windows startup for continuous Linux disk detection and native Explorer integration.

---

## User Review Required

> [!IMPORTANT]
> **Privilege & Startup Architecture**:
> - Reading raw physical disks (`\\.\PhysicalDriveN`) requires Administrator privileges on Windows.
> - When the user grants permission to run on startup, the installer/daemon registers an auto-start entry using **Windows Task Scheduler** (`SCHTASKS`) with `HighestAvailable` run level (or registry run key with UAC elevation). This ensures the background resident monitor starts automatically on Windows logon without displaying disruptive UAC popups every boot.

> [!NOTE]
> **Windows Explorer Integration**:
> - Creates and manages a dedicated `C:\LinuxDisks` root folder with desktop/Explorer shortcuts.
> - Hotplugged Linux partitions (Ext2/3/4) are automatically projected into `C:\LinuxDisks\Disk<N>_Part<M>` using ProjFS.
> - Users can double-click, view properties, drag-and-drop, and copy files/folders directly in Windows Explorer.

---

## Proposed Components & Changes

### 1. Memory-Resident Daemon & System Tray (`src/service/` & `src/vfs/`)
- **`linux2win_daemon.exe` / `linux2win --daemon`**:
  - Background memory-resident process with Win32 System Tray Icon (`Shell_NotifyIconW`).
  - Listens for disk insertion / removal in real-time.
  - Automatically mounts Linux data partitions to `C:\LinuxDisks\Disk<N>_Part<M>` using ProjFS.
  - Displays desktop balloon notifications when Linux disks are inserted or removed.
  - Tray Context Menu:
    - 📂 *Open Linux Disks in Explorer*
    - 🔍 *Rescan Disks Now*
    - ✔️ *Run on Windows Startup* (Toggleable permission)
    - ℹ️ *Safety & Partition Status*
    - ❌ *Exit*

### 2. User Permission & Consent Manager (`include/linux2win/startup_manager.hpp`, `src/service/startup_manager.cpp`)
- Checks if startup permission has been granted by user.
- If not yet configured, shows a modern Win32 Consent Dialog:
  > *"Linux2Win can run in the background to automatically detect Linux disks/USBs when inserted and show them in Windows Explorer for safe read-only browsing and file copying. Would you like to enable this?"*
- Configures / removes Task Scheduler auto-start and Registry hooks.

### 3. Windows Installer Wizard (`src/installer/`)
- **`linux2win_setup.exe`**:
  - Native standalone GUI installer built with CMake and Win32.
  - **Step 1**: Welcome & Safety Overview (read-only safety guarantees).
  - **Step 2**: Installation folder selection (default: `C:\Program Files\Linux2Win`).
  - **Step 3**: **Interactive Startup Permission Prompt**:
    - [x] *Load Linux2Win in background on Windows startup (Recommended)*
    - [x] *Create Windows Explorer shortcut to C:\LinuxDisks*
    - [x] *Enable Windows ProjFS optional feature if not active*
  - **Step 4**: File installation & registration.
  - **Step 5**: Creation of `uninstall.exe` uninstaller.
  - **Step 6**: Launch background resident service immediately upon completion.

### 4. CMake & Packaging Updates
- Add `linux2win_daemon` and `linux2win_setup` targets in `CMakeLists.txt`.
- Add CPack configuration for ZIP and MSI package generation.

---

## Verification Plan

### Automated Build & Unit Tests
- Compile all targets (`linux2win`, `linux2win_daemon`, `linux2win_setup`, unit tests) with MSVC in Release mode.
- Run CTest suite to ensure all unit tests pass.

### Functional Verification
- Verify `linux2win_setup.exe` displays the setup wizard and permission prompt.
- Verify startup manager can register and query Task Scheduler / Registry startup state.
- Verify System Tray icon creation, context menu, and auto-mount notifications.
- Verify uninstaller removes startup entries, files, and shortcuts cleanly.
