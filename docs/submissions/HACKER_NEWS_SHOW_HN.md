# Hacker News (Show HN) Submission

**URL**: https://news.ycombinator.com/submit

### Title:
```text
Show HN: Linux2Win – Auto-detect Linux disks and mount to Windows Explorer (Ext4/ProjFS)
```

### URL to submit:
```text
https://github.com/APS-PPR-2016/Linux2Win
```

### Text (Comment / Discussion Starter):
```text
Hey HN,

I built Linux2Win (C++20, CMake), an open-source tool that monitors physical and USB drives on Windows, automatically detects Linux data partitions (Ext2/Ext3/Ext4), and projects them directly into Windows File Explorer via the Windows Projected File System (ProjFS).

Key design choices & safety guarantees:
1. Data-only partition filter: Automatically inspects GPT/MBR partition tables and strictly ignores/excludes EFI System Partitions (ESP), BIOS boot, Linux /boot, and Swap partitions.
2. Read-only safety protection: Disk handles are opened strictly in GENERIC_READ mode, files are tagged FILE_ATTRIBUTE_READONLY, and any write/delete attempts return ERROR_ACCESS_DENIED to prevent corruption.
3. Zero third-party drivers needed: Uses Windows native ProjFS (no kernel driver like Dokan/WinFsp required) to project ext4 directly into C:\LinuxDisks\DiskN_PartM.
4. Memory-resident daemon: Includes a System Tray monitor (with user startup consent) that detects USB insertions in real-time and shows balloon notifications.

Source & Releases: https://github.com/APS-PPR-2016/Linux2Win
Release v1.0.0 installer: https://github.com/APS-PPR-2016/Linux2Win/releases/tag/v1.0.0

Feedback, bug reports, and questions are welcome!
```
