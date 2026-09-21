# Linux2Win 🚀

[![CI/CD Build and Release](https://github.com/APS-PPR-2016/Linux2Win/actions/workflows/ci-build-and-release.yml/badge.svg)](https://github.com/APS-PPR-2016/Linux2Win/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6?logo=windows)](https://github.com/APS-PPR-2016/Linux2Win)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-APS--2016-FFDD00?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/aps2016)

A modern, high-performance C++20 / CMake solution that automatically detects Linux disks & USBs (when inserted or already present), validates and parses Linux data partitions (Ext2/3/4), strictly excludes boot/system/swap partitions, protects write safety with read-only enforcement, and projects data partitions as native Windows folders using **Windows Projected File System (ProjFS)**.

---

## 🌟 Key Features

1. **Automatic Disk & Hotplug Monitoring**:
   - Detects all existing physical drives (`PhysicalDrive0` .. `PhysicalDriveN`).
   - Listens in real-time for USB and external disk insertion and removal via Win32 `WM_DEVICECHANGE` device interface hooks.
2. **Strict Data-Only Partition Filtering**:
   - Automatically inspects GPT and MBR partition tables.
   - **Allowed Data Partitions**: Linux Filesystem Data, Linux `/home`, Linux Root (`x86_64`, `arm64`, `x86`), `/srv`, `/var`, and user data partitions.
   - **Excluded Partitions**: EFI System Partitions (ESP), BIOS Boot, Linux `/boot`, Linux Swap, Microsoft Reserved (MSR), Microsoft Recovery.
3. **Zero-Dependency Ext2/Ext3/Ext4 Engine**:
   - Built-in, self-contained parser supporting 64-bit block addressing, extents trees, directory hash/linear entries, fast & regular symlinks, and inode table parsing.
4. **Native Windows Explorer Integration (ProjFS)**:
   - Projects the Linux filesystem directly into `C:\LinuxDisks\Disk<N>_Part<M>` using the Windows Projected File System.
   - Standard Windows Explorer operations: double-click to open files, view properties, drag-and-drop, and copy files/folders directly to Windows drives.
5. **Enforced Read-Only Safety**:
   - Drive handles opened strictly in `GENERIC_READ` mode with `FILE_SHARE_READ | FILE_SHARE_WRITE`.
   - File attributes tagged with `FILE_ATTRIBUTE_READONLY`.
   - All write, modify, truncate, and delete operations are blocked (`ERROR_ACCESS_DENIED`).
6. **Memory-Resident Background Daemon & System Tray**:
   - Runs quietly in the notification area with a sleek context menu.
   - Interactive permission prompt asking user consent for Windows startup.
   - Desktop balloon notifications when Linux partitions are mounted or unmounted.

---

## 📦 Windows Setup & Installation

Linux2Win provides an integrated Windows Setup Wizard (`linux2win_setup.exe`):

1. **Interactive Startup Permission**: Asks for permission to load on Windows startup in memory-resident background mode.
2. **Explorer Shortcuts**: Automatically creates desktop and Start Menu shortcuts to `C:\LinuxDisks`.
3. **ProjFS Feature Activator**: Enables the Windows Projected File System feature if not active.
4. **Clean Uninstallation**: Registers with Windows *Add or Remove Programs* and generates `uninstall.exe`.

---

## 💻 CLI Usage

```powershell
# Scan all physical drives and classify partitions (shows data vs excluded boot/swap partitions)
.\linux2win.exe scan

# Start real-time hotplug monitor (detects USB / drive insertion and removal)
.\linux2win.exe monitor

# Auto-mount all detected Linux data partitions to Windows folders
.\linux2win.exe auto

# Mount a specific Linux data partition as a Windows folder via ProjFS
.\linux2win.exe mount <disk_index> <partition_number> [target_folder]
# Example:
.\linux2win.exe mount 1 2 C:\LinuxDisks\Drive1_Part2

# List directory contents on a Linux partition
.\linux2win.exe ls <disk_index> <partition_number> [path]
# Example:
.\linux2win.exe ls 1 2 /home/user

# Display contents of a file on Linux partition
.\linux2win.exe cat <disk_index> <partition_number> <file_path>

# Safely copy/export a file or folder from Linux partition to Windows
.\linux2win.exe export <disk_index> <partition_number> <linux_path> <windows_target_dir>
# Example:
.\linux2win.exe export 1 2 /etc/fstab C:\Users\YourUser\Desktop
```

---

## 🛠️ Building from Source

### Prerequisites
- Visual Studio 2022 (with MSVC C++20 toolchain)
- CMake 3.20+
- Windows 10 (1809+) or Windows 11

### Build Instructions
```powershell
# Clone the repository
git clone https://github.com/APS-PPR-2016/Linux2Win.git
cd Linux2Win

# Configure with MSVC 2022
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release binaries
cmake --build build --config Release

# Run automated unit tests
cd build
ctest -C Release --output-on-failure

# Package ZIP distribution
cpack -C Release -G ZIP
```

---

## ☕ Support the Project

If you find **Linux2Win** useful, please consider supporting the project:

<div align="center">
  <a href="https://buymeacoffee.com/aps2016" target="_blank">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" width="200"/>
  </a>
  <br/><br/>
  <a href="https://buymeacoffee.com/aps2016" target="_blank">
    <img src="assets/buymeacoffee_qr.png" alt="Buy Me A Coffee QR Code" width="180"/>
  </a>
  <br/>
  <b>Scan or click the QR code to Buy Me A Coffee at <a href="https://buymeacoffee.com/aps2016">buymeacoffee.com/aps2016</a>!</b>
</div>

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
