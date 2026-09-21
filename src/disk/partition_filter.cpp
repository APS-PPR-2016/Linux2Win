#include "linux2win/partition_filter.hpp"
#include "linux2win/ext4_types.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace linux2win {

bool Guid::operator==(const Guid& other) const noexcept {
    return data1 == other.data1 &&
           data2 == other.data2 &&
           data3 == other.data3 &&
           std::memcmp(data4, other.data4, sizeof(data4)) == 0;
}

std::string Guid::to_string() const {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << data1 << "-"
       << std::setw(4) << data2 << "-"
       << std::setw(4) << data3 << "-"
       << std::setw(2) << static_cast<int>(data4[0])
       << std::setw(2) << static_cast<int>(data4[1]) << "-"
       << std::setw(2) << static_cast<int>(data4[2])
       << std::setw(2) << static_cast<int>(data4[3])
       << std::setw(2) << static_cast<int>(data4[4])
       << std::setw(2) << static_cast<int>(data4[5])
       << std::setw(2) << static_cast<int>(data4[6])
       << std::setw(2) << static_cast<int>(data4[7]);
    return ss.str();
}

Guid Guid::from_string(const std::string& str) {
    Guid g{};
    // Remove dashes and convert to lower
    std::string clean;
    for (char c : str) {
        if (c != '-' && c != '{' && c != '}') {
            clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (clean.length() != 32) return g;

    try {
        g.data1 = static_cast<uint32_t>(std::stoul(clean.substr(0, 8), nullptr, 16));
        g.data2 = static_cast<uint16_t>(std::stoul(clean.substr(8, 4), nullptr, 16));
        g.data3 = static_cast<uint16_t>(std::stoul(clean.substr(12, 4), nullptr, 16));
        for (size_t i = 0; i < 8; ++i) {
            g.data4[i] = static_cast<uint8_t>(std::stoul(clean.substr(16 + i * 2, 2), nullptr, 16));
        }
    } catch (...) {
        return Guid{};
    }
    return g;
}

PartitionKind PartitionFilter::classify_gpt(const Guid& type_guid) {
    // Check excluded boot / system GUIDs first
    if (type_guid == guids::EfiSystemPartition) return PartitionKind::EfiSystem;
    if (type_guid == guids::BiosBoot)           return PartitionKind::BiosBoot;
    if (type_guid == guids::LinuxBoot)          return PartitionKind::LinuxBoot;
    if (type_guid == guids::LinuxSwap)          return PartitionKind::LinuxSwap;
    if (type_guid == guids::WindowsMSR)         return PartitionKind::WindowsMSR;
    if (type_guid == guids::WindowsRecovery)    return PartitionKind::WindowsRecovery;

    // Check safe Linux data GUIDs
    if (type_guid == guids::LinuxGenericData)   return PartitionKind::LinuxData;
    if (type_guid == guids::LinuxRootX86_64)    return PartitionKind::LinuxRoot;
    if (type_guid == guids::LinuxRootArm64)     return PartitionKind::LinuxRoot;
    if (type_guid == guids::LinuxRootX86)       return PartitionKind::LinuxRoot;
    if (type_guid == guids::LinuxHome)          return PartitionKind::LinuxHome;
    if (type_guid == guids::LinuxSrv)           return PartitionKind::LinuxSrv;
    if (type_guid == guids::LinuxVar)           return PartitionKind::LinuxVar;
    if (type_guid == guids::LinuxVarTmp)        return PartitionKind::LinuxVar;
    if (type_guid == guids::LinuxUserHome)      return PartitionKind::LinuxHome;

    return PartitionKind::Unknown;
}

PartitionKind PartitionFilter::classify_mbr(uint8_t mbr_type, bool is_bootable) {
    switch (mbr_type) {
        case 0x83: // Linux native partition (ext2/3/4, btrfs, xfs)
            return PartitionKind::LinuxData;
        case 0x82: // Linux swap
            return PartitionKind::LinuxSwap;
        case 0xEF: // EFI System Partition
            return PartitionKind::EfiSystem;
        case 0x07: // NTFS / exFAT / Windows Basic Data
            return PartitionKind::WindowsData;
        case 0x27: // Windows RE / Recovery
            return PartitionKind::WindowsRecovery;
        default:
            return PartitionKind::Unknown;
    }
}

bool PartitionFilter::is_safe_data_partition(PartitionKind kind) {
    switch (kind) {
        case PartitionKind::LinuxData:
        case PartitionKind::LinuxRoot:
        case PartitionKind::LinuxHome:
        case PartitionKind::LinuxSrv:
        case PartitionKind::LinuxVar:
            return true;
        default:
            return false;
    }
}

bool PartitionFilter::is_excluded_system_partition(PartitionKind kind) {
    switch (kind) {
        case PartitionKind::EfiSystem:
        case PartitionKind::BiosBoot:
        case PartitionKind::LinuxBoot:
        case PartitionKind::LinuxSwap:
        case PartitionKind::WindowsMSR:
        case PartitionKind::WindowsRecovery:
            return true;
        default:
            return false;
    }
}

std::string PartitionFilter::kind_to_string(PartitionKind kind) {
    switch (kind) {
        case PartitionKind::LinuxData:       return "Linux Filesystem Data";
        case PartitionKind::LinuxRoot:       return "Linux Root";
        case PartitionKind::LinuxHome:       return "Linux /home";
        case PartitionKind::LinuxSrv:        return "Linux /srv";
        case PartitionKind::LinuxVar:        return "Linux /var";
        case PartitionKind::LinuxBoot:       return "Linux /boot (Excluded)";
        case PartitionKind::LinuxSwap:       return "Linux Swap (Excluded)";
        case PartitionKind::EfiSystem:       return "EFI System Partition (Excluded)";
        case PartitionKind::BiosBoot:        return "BIOS Boot (Excluded)";
        case PartitionKind::WindowsMSR:      return "Microsoft Reserved (Excluded)";
        case PartitionKind::WindowsData:     return "Windows Basic Data";
        case PartitionKind::WindowsRecovery: return "Windows Recovery (Excluded)";
        default:                             return "Unknown";
    }
}

FilesystemType PartitionFilter::probe_filesystem(const uint8_t* sector_buffer, size_t buffer_size) {
    if (!sector_buffer || buffer_size < 2048) return FilesystemType::Unknown;

    // Ext2/3/4 superblock is at byte offset 1024 (0x400)
    const auto* sb = reinterpret_cast<const fs::Ext4Superblock*>(sector_buffer + fs::EXT4_SUPERBLOCK_OFFSET);
    if (sb->s_magic == fs::EXT4_SUPERBLOCK_MAGIC) {
        // Distinguish ext2, ext3, ext4
        if ((sb->s_feature_incompat & fs::EXT4_FEATURE_INCOMPAT_EXTENTS) != 0 ||
            (sb->s_feature_incompat & fs::EXT4_FEATURE_INCOMPAT_64BIT) != 0) {
            return FilesystemType::Ext4;
        }
        if ((sb->s_feature_compat & 0x0004) != 0) { // EXT3_FEATURE_COMPAT_HAS_JOURNAL
            return FilesystemType::Ext3;
        }
        return FilesystemType::Ext2;
    }

    // Check Btrfs magic at offset 64KB (0x10000) or check within first buffer if available
    // Btrfs superblock magic is "_BHRfS_M" at offset 0x10040 (offset 64 from 64KB superblock)
    // If buffer is large enough:
    if (buffer_size >= 0x10048) {
        if (std::memcmp(sector_buffer + 0x10040, "_BHRfS_M", 8) == 0) {
            return FilesystemType::Btrfs;
        }
    }

    return FilesystemType::Unknown;
}

std::string PartitionFilter::fs_type_to_string(FilesystemType fs) {
    switch (fs) {
        case FilesystemType::Ext2:   return "Ext2";
        case FilesystemType::Ext3:   return "Ext3";
        case FilesystemType::Ext4:   return "Ext4";
        case FilesystemType::Btrfs:  return "Btrfs";
        case FilesystemType::Xfs:    return "XFS";
        default:                     return "Unknown";
    }
}

} // namespace linux2win
