#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace linux2win {

struct Guid {
    uint32_t data1{0};
    uint16_t data2{0};
    uint16_t data3{0};
    uint8_t  data4[8]{0};

    bool operator==(const Guid& other) const noexcept;
    bool operator!=(const Guid& other) const noexcept { return !(*this == other); }
    std::string to_string() const;
    static Guid from_string(const std::string& str);
};

enum class PartitionKind {
    Unknown,
    LinuxData,       // Linux filesystem data (ext4, btrfs, xfs, etc.)
    LinuxRoot,       // Linux root partition
    LinuxHome,       // Linux /home
    LinuxSrv,        // Linux /srv
    LinuxVar,        // Linux /var
    LinuxBoot,       // Linux /boot (Excluded for safety)
    LinuxSwap,       // Linux swap (Excluded)
    EfiSystem,       // EFI ESP (Excluded)
    BiosBoot,        // BIOS Boot (Excluded)
    WindowsMSR,      // Microsoft Reserved (Excluded)
    WindowsData,     // Basic Data (NTFS/FAT32)
    WindowsRecovery  // Recovery partition (Excluded)
};

enum class FilesystemType {
    Unknown,
    Ext2,
    Ext3,
    Ext4,
    Btrfs,
    Xfs
};

struct PartitionInfo {
    uint32_t disk_index{0};
    uint32_t partition_number{0};
    uint64_t starting_offset{0};
    uint64_t total_length{0};
    bool is_gpt{false};
    Guid gpt_type_guid{};
    Guid gpt_unique_id{};
    uint8_t mbr_type{0};
    bool is_bootable{false};
    PartitionKind kind{PartitionKind::Unknown};
    FilesystemType fs_type{FilesystemType::Unknown};
    std::string name;
    std::string fs_label;
    std::string fs_uuid;
    bool is_data_partition{false}; // true if safe to read and mount
};

struct DiskInfo {
    uint32_t disk_index{0};
    std::string device_path;         // e.g. \\.\PhysicalDrive0
    std::string friendly_name;       // Model / Vendor
    std::string serial_number;
    std::string bus_type;            // USB, NVMe, SATA, SCSI
    uint64_t total_size_bytes{0};
    uint32_t sector_size{512};
    bool is_removable{false};
    bool has_gpt{false};
    std::vector<PartitionInfo> partitions;
};

// Known Partition GUIDs
namespace guids {
    // Linux Data & Home
    inline const Guid LinuxGenericData   = Guid::from_string("0fc63daf-8483-4772-8e79-3d69d8477de4");
    inline const Guid LinuxRootX86_64    = Guid::from_string("4f68bce3-e8cd-4db1-96e7-fbcaf984b709");
    inline const Guid LinuxRootArm64     = Guid::from_string("69dad710-2ce4-4e3c-b16c-21a1d49abed3");
    inline const Guid LinuxRootX86       = Guid::from_string("44479540-f297-412e-ba79-2567842e16c2");
    inline const Guid LinuxHome          = Guid::from_string("933ac7e2-d68e-4444-bd25-ce80fdb68037");
    inline const Guid LinuxSrv           = Guid::from_string("3b8f8425-20e0-4f3b-814a-112e00142f12");
    inline const Guid LinuxVar           = Guid::from_string("4d21b016-b534-45c2-a9fb-5c16e091fd2d");
    inline const Guid LinuxVarTmp        = Guid::from_string("7ec6f557-3bc5-4aca-b293-16ef5df639d1");
    inline const Guid LinuxUserHome      = Guid::from_string("773f0e24-5d23-4d7a-cd77-61047cb5d029");

    // System / Boot (MUST BE EXCLUDED)
    inline const Guid EfiSystemPartition = Guid::from_string("c12a7328-f81f-11d2-ba4b-00a0c93ec93b");
    inline const Guid BiosBoot           = Guid::from_string("21686148-6449-6e6f-744e-656564454649");
    inline const Guid LinuxBoot          = Guid::from_string("bc13c2ff-59e6-4262-a352-b275fd6f7172");
    inline const Guid LinuxSwap          = Guid::from_string("0657fd6d-a4ab-43c4-84e5-0933c84b4f4f");
    inline const Guid WindowsMSR         = Guid::from_string("e3c9e310-0b23-11d1-a4a4-00aa00c76742");
    inline const Guid WindowsRecovery    = Guid::from_string("de94bba4-06d1-4d40-a16a-bfd50179d6ac");
}

} // namespace linux2win
