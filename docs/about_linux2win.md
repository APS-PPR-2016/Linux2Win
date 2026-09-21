# Walkthrough: Linux2Win CMake Project

The **Linux2Win** project has been successfully designed, implemented, and verified. It provides a complete, modern C++20 / CMake solution for detecting Linux disks, safely filtering data partitions, and projecting them as native Windows folders.

---

## What Was Created

### 1. Project Architecture & Components

| Component | Files | Description |
| :--- | :--- | :--- |
| **Data Types & GUIDs** | [types.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/types.hpp) | Defines GUIDs for GPT/MBR partition types, `DiskInfo`, `PartitionInfo`, `Guid` parser. |
| **Partition Filter** | [partition_filter.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/partition_filter.hpp), [partition_filter.cpp](file:///e:/GNeuralSystems/Linux2Win/src/disk/partition_filter.cpp) | Strictly distinguishes Linux data partitions from boot/system/swap partitions. Probes Ext2/3/4 & Btrfs headers. |
| **Disk Enumeration** | [disk_enumerator.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/disk_enumerator.hpp), [disk_enumerator.cpp](file:///e:/GNeuralSystems/Linux2Win/src/disk/disk_enumerator.cpp) | Reads drive layouts (`IOCTL_DISK_GET_DRIVE_LAYOUT_EX`), disk geometry, and hardware properties. |
| **Hotplug Monitor** | [disk_monitor.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/disk_monitor.hpp), [disk_monitor.cpp](file:///e:/GNeuralSystems/Linux2Win/src/disk/disk_monitor.cpp) | Win32 message loop window listening for `WM_DEVICECHANGE` (`DBT_DEVICEARRIVAL`, `DBT_DEVICEREMOVECOMPLETE`). |
| **Block Device** | [block_device.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/block_device.hpp), [block_device.cpp](file:///e:/GNeuralSystems/Linux2Win/src/fs/block_device.cpp) | Safe read-only sector-aligned disk block reader. Prevents write operations. |
| **Ext4 Engine** | [ext4_types.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/ext4_types.hpp), [ext4_reader.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/ext4_reader.hpp), [ext4_reader.cpp](file:///e:/GNeuralSystems/Linux2Win/src/fs/ext4_reader.cpp) | Self-contained Ext2/3/4 reader: superblocks, 64-bit block descriptors, extents tree, directory indexing, inodes, and symlinks. |
| **VFS Tree** | [vfs_tree.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/vfs_tree.hpp), [vfs_tree.cpp](file:///e:/GNeuralSystems/Linux2Win/src/vfs/vfs_tree.cpp) | In-memory hierarchical directory representation, path normalization, Win32 epoch conversion, and caching. |
| **ProjFS Projection** | [projfs_mount.hpp](file:///e:/GNeuralSystems/Linux2Win/include/linux2win/projfs_mount.hpp), [projfs_mount.cpp](file:///e:/GNeuralSystems/Linux2Win/src/vfs/projfs_mount.cpp) | Windows Projected File System provider with on-demand dynamic API loading and write-protection hooks. |
| **CLI Application** | [main.cpp](file:///e:/GNeuralSystems/Linux2Win/src/main.cpp) | Command-line interface supporting `scan`, `monitor`, `auto`, `mount`, `ls`, `cat`, `export`. |
| **Build System** | [CMakeLists.txt](file:///e:/GNeuralSystems/Linux2Win/CMakeLists.txt), [README.md](file:///e:/GNeuralSystems/Linux2Win/README.md) | CMake C++20 build definition, test registration, install rules, documentation. |

---

## Safety & Partition Filtering Policy

### Partition Handling
- **Allowed Data Partitions (Safe to Read/Mount)**:
  - Linux Generic Filesystem Data (`0fc63daf-8483-4772-8e79-3d69d8477de4` / MBR `0x83`)
  - Linux Root (`x86_64`, `arm64`, `x86`)
  - Linux `/home` (`933ac7e2-d68e-4444-bd25-ce80fdb68037`)
  - Linux `/srv`, `/var`, and user data partitions
- **Excluded Boot & System Partitions (Blocked for Safety)**:
  - EFI System Partition (`c12a7328-f81f-11d2-ba4b-00a0c93ec93b` / MBR `0xEF`)
  - BIOS Boot (`21686148-6449-6e6f-744e-656564454649`)
  - Linux `/boot` (`bc13c2ff-59e6-4262-a352-b275fd6f7172`)
  - Linux Swap (`0657fd6d-a4ab-43c4-84e5-0933c84b4f4f` / MBR `0x82`)
  - Microsoft Reserved / MSR & Recovery partitions

### Read-Only Safety Guarantees
- Raw disk handles are opened strictly with `GENERIC_READ` mode and `FILE_SHARE_READ | FILE_SHARE_WRITE`.
- Projected files in Windows are marked with `FILE_ATTRIBUTE_READONLY`.
- All write, overwrite, rename, and delete callbacks in the VFS layer return `ERROR_ACCESS_DENIED`.

---

## Verification Results

### Automated Unit Tests
Executed via CTest with 100% pass rate:
```
Test project E:/GNeuralSystems/Linux2Win/build
    Start 1: PartitionFilterTest
1/3 Test #1: PartitionFilterTest ..............   Passed    0.84 sec
    Start 2: Ext4StructuresTest
2/3 Test #2: Ext4StructuresTest ...............   Passed    0.54 sec
    Start 3: VfsTreeTest
3/3 Test #3: VfsTreeTest ......................   Passed    0.53 sec

100% tests passed, 0 tests failed out of 3
```

### CLI Command Verification
- Executed `.\Release\linux2win.exe` with banner, help, and commands successfully displayed.
- Executed `.\Release\linux2win.exe scan` with drive enumeration logic tested.
