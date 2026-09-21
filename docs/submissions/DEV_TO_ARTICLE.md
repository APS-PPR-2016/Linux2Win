# Dev.to Article Draft

**Platform**: https://dev.to/new

### Frontmatter:
```yaml
---
title: Building Linux2Win: Auto-Detecting Linux Disks & Mounting to Windows Explorer with ProjFS in C++20
published: true
tags: cpp, windows, linux, opensource
canonical_url: https://github.com/APS-PPR-2016/Linux2Win
cover_image: https://raw.githubusercontent.com/APS-PPR-2016/Linux2Win/main/assets/buymeacoffee_qr.png
---
```

### Article Content:
```markdown
Connecting a Linux-formatted hard drive or USB (ext4) on Windows has historically been a painful experience. Most users end up with outdated, unsigned kernel drivers that can cause Blue Screens (BSODs) or risk filesystem corruption.

To solve this cleanly, I built **Linux2Win** — an open-source C++20 application that monitors storage devices, filters out system partitions, and mounts Linux ext4 data partitions directly into Windows File Explorer using the native **Windows Projected File System (ProjFS)**.

---

## 🎯 Architecture & Safety

### 1. Zero Third-Party Kernel Drivers
Instead of installing kernel-level file system drivers (like Dokany or WinFsp), Linux2Win uses Windows 10/11 native **Windows Projected File System** (`projectedfslib`). ProjFS is a user-mode filesystem virtualization API introduced by Microsoft, allowing applications to project virtual hierarchies directly into native Windows folders.

### 2. Strict Read-Only Safety Guarantees
One of the biggest hazards of accessing Linux drives in Windows is accidental writes or NTFS indexers modifying the filesystem. Linux2Win enforces read-only access at three independent levels:
- **Win32 Handle Level**: Physical drive handles are opened strictly with `GENERIC_READ` and `FILE_SHARE_READ | FILE_SHARE_WRITE`.
- **Attribute Level**: All projected directories and files are tagged with `FILE_ATTRIBUTE_READONLY`.
- **Callback Level**: Any write, overwrite, rename, or delete callback immediately returns `ERROR_ACCESS_DENIED`.

### 3. Intelligent Partition Filtering
Not all partitions on a Linux disk contain user files. Linux2Win parses GPT and MBR partition tables and strictly excludes:
- EFI System Partitions (ESP)
- BIOS Boot Partitions
- Linux `/boot` partitions
- Linux Swap partitions
- Microsoft Reserved (MSR) and Recovery partitions

Only actual Linux filesystem data partitions (root, `/home`, `/srv`, `/var`, user data) are mounted.

---

## 🚀 Memory-Resident Daemon & System Tray

Linux2Win includes an optional background daemon (`linux2win_daemon.exe`) that:
- Runs in the Windows System Tray with a clean context menu.
- Hooks `WM_DEVICECHANGE` (`GUID_DEVINTERFACE_DISK`) for instant hotplug mounting.
- Shows Windows desktop notifications when a Linux disk is attached.
- Creates `C:\LinuxDisks` desktop and Explorer shortcuts for quick access.

---

## 📦 Try It Out

- **GitHub Repository**: [https://github.com/APS-PPR-2016/Linux2Win](https://github.com/APS-PPR-2016/Linux2Win)
- **Latest Release**: [https://github.com/APS-PPR-2016/Linux2Win/releases/tag/v1.0.0](https://github.com/APS-PPR-2016/Linux2Win/releases/tag/v1.0.0)

If you find this project helpful, check out the repository, star it on GitHub, or support the project at [buymeacoffee.com/aps2016](https://buymeacoffee.com/aps2016)!
```
