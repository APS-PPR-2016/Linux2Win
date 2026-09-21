# Reddit Submissions Pack

---

### 1. `r/opensource` & `r/sideproject`

**Title**: 
`[Open Source] Linux2Win: Auto-detect Linux disks and mount them to Windows Explorer as native folders (C++20, Ext4, ProjFS)`

**Post Body**:
```markdown
Hey everyone!

I created **Linux2Win**, an open-source C++20 tool that automatically detects Linux disks/USBs (on insertion or existing) and projects their ext2/ext3/ext4 partitions directly into Windows File Explorer via the Windows Projected File System (ProjFS).

### Why I built it:
Reading Linux ext4 drives on Windows usually meant installing unsigned third-party kernel drivers (like Ext2Fsd/WinFsp/Dokany) or booting a WSL2 VM. Linux2Win takes a clean approach:
1. **Zero kernel drivers**: Uses Windows 10/11 native user-mode ProjFS API.
2. **Strict Read-Only Protection**: Disk handles are opened in `GENERIC_READ` mode and write operations are blocked to prevent data loss.
3. **Partition Filter**: Strictly excludes boot partitions (EFI ESP, BIOS boot, /boot, and Swap) to protect system partitions and only mounts data.
4. **Memory-Resident Daemon**: Runs in the System Tray (with user startup consent) and sends balloon notifications when a Linux USB is plugged in.
5. **Windows Explorer Integration**: Browse directories, drag-and-drop, and copy files natively from `C:\LinuxDisks`.

- **GitHub Repository**: https://github.com/APS-PPR-2016/Linux2Win
- **Release v1.0.0**: https://github.com/APS-PPR-2016/Linux2Win/releases/tag/v1.0.0
- **License**: MIT

I’d love to hear your thoughts and feedback!
```

---

### 2. `r/cpp`

**Title**:
`Linux2Win: A C++20 / CMake tool implementing user-mode Ext4 filesystem projection to Windows Explorer using ProjFS`

**Post Body**:
```markdown
Hi r/cpp,

I’ve open-sourced **Linux2Win**, a modern C++20 project that parses Ext2/Ext3/Ext4 on physical drives and USB devices in user-mode and projects the filesystem hierarchy directly into Windows Explorer using Windows Projected File System (`projectedfslib`).

Key Technical Details:
- **C++20 & CMake**: Modular design with clean separation of raw block device reader, on-disk ext4 struct parsers (64-bit block group descriptors, extents tree, directory indexing, symlinks), and VFS hierarchy tree.
- **Dynamic ProjFS Loader**: Dynamically loads `ProjectedFSLib.dll` at runtime to ensure zero hard DLL dependencies when the optional feature is not yet active.
- **Win32 Hotplug Monitoring**: Uses a hidden message window listening for `WM_DEVICECHANGE` (`GUID_DEVINTERFACE_DISK`) for instant hotplug mounting.
- **Safety Enforcement**: Hardened read-only guarantees preventing raw sector writes or virtualized modifications.

Code & Architecture: https://github.com/APS-PPR-2016/Linux2Win

Any code review, suggestions, or contributions are very welcome!
```

---

### 3. `r/windows` & `r/linux`

**Title**:
`Easily read and copy files from Linux Ext4 disks & USBs in Windows Explorer without 3rd party drivers (Linux2Win)`

**Post Body**:
```markdown
If you dual boot Linux & Windows or frequently move Linux-formatted USB drives between machines, accessing files on Windows without corrupting ext4 can be tricky.

I wrote **Linux2Win**, an open-source tool for Windows 10 & 11 that:
- Automatically detects Linux drives when you plug them in.
- Shows ext2/ext3/ext4 partitions directly in Windows File Explorer under `C:\LinuxDisks`.
- Enables normal Windows file copying, opening, and browsing.
- Protects your partitions with enforced read-only safety (no risk of Windows corrupting your Linux filesystem).
- Automatically skips boot, EFI, and swap partitions.

Free and open-source under MIT:
GitHub: https://github.com/APS-PPR-2016/Linux2Win
Releases: https://github.com/APS-PPR-2016/Linux2Win/releases
```
