#pragma once

#include "linux2win/types.hpp"
#include <string>

namespace linux2win {

class PartitionFilter {
public:
    // Analyzes and classifies a GPT or MBR partition
    static PartitionKind classify_gpt(const Guid& type_guid);
    static PartitionKind classify_mbr(uint8_t mbr_type, bool is_bootable);

    // Returns true if the partition is a data partition and SAFE to mount/read
    static bool is_safe_data_partition(PartitionKind kind);

    // Returns human readable description of partition kind
    static std::string kind_to_string(PartitionKind kind);

    // Determines if a partition should be strictly ignored (Boot, ESP, Swap, MSR, Recovery)
    static bool is_excluded_system_partition(PartitionKind kind);

    // Checks magic numbers at beginning of partition to identify Ext2/3/4 or Btrfs
    static FilesystemType probe_filesystem(const uint8_t* sector_buffer, size_t buffer_size);

    static std::string fs_type_to_string(FilesystemType fs);
};

} // namespace linux2win
