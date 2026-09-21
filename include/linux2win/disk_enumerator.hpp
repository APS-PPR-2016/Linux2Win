#pragma once

#include "linux2win/types.hpp"
#include <vector>
#include <string>
#include <optional>

namespace linux2win {

class DiskEnumerator {
public:
    // Enumerates all physical drives present in the system
    static std::vector<DiskInfo> enumerate_all_disks();

    // Inspects a single physical disk by index (e.g. 0 for \\.\PhysicalDrive0)
    static std::optional<DiskInfo> inspect_disk(uint32_t disk_index);

    // Filter only data partitions from disk info list
    static std::vector<PartitionInfo> get_all_mountable_partitions();

    // Helper: convert Windows drive layout to PartitionInfo list
    static bool query_disk_layout(HANDLE hDisk, DiskInfo& disk);

    // Helper: query disk properties (bus type, vendor, product, removable)
    static bool query_disk_properties(HANDLE hDisk, DiskInfo& disk);

    // Probes first sector of partition to detect filesystem (Ext4, etc.) and volume metadata
    static void probe_partition_filesystem(HANDLE hDisk, PartitionInfo& part);
};

} // namespace linux2win
