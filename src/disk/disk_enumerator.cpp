#include "linux2win/disk_enumerator.hpp"
#include "linux2win/partition_filter.hpp"
#include "linux2win/ext4_types.hpp"
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>

#pragma comment(lib, "setupapi.lib")

namespace linux2win {

static Guid win_guid_to_guid(const GUID& wg) {
    Guid g;
    g.data1 = wg.Data1;
    g.data2 = wg.Data2;
    g.data3 = wg.Data3;
    std::memcpy(g.data4, wg.Data4, 8);
    return g;
}

std::vector<DiskInfo> DiskEnumerator::enumerate_all_disks() {
    std::vector<DiskInfo> disks;
    int consecutive_misses = 0;

    for (uint32_t i = 0; i < 32; ++i) {
        auto disk_opt = inspect_disk(i);
        if (disk_opt.has_value()) {
            disks.push_back(std::move(disk_opt.value()));
            consecutive_misses = 0;
        } else {
            consecutive_misses++;
            if (consecutive_misses >= 3 && i >= 4) {
                break; // No more active physical drives attached
            }
        }
    }

    return disks;
}

std::optional<DiskInfo> DiskEnumerator::inspect_disk(uint32_t disk_index) {
    std::string path = "\\\\.\\PhysicalDrive" + std::to_string(disk_index);
    std::wstring wpath(path.begin(), path.end());

    // Strictly open with GENERIC_READ and FILE_SHARE_READ | FILE_SHARE_WRITE
    HANDLE hDisk = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDisk == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    DiskInfo disk;
    disk.disk_index = disk_index;
    disk.device_path = path;

    // Query disk geometry & size
    DISK_GEOMETRY_EX geom{};
    DWORD bytesReturned = 0;
    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr)) {
        disk.total_size_bytes = static_cast<uint64_t>(geom.DiskSize.QuadPart);
        disk.sector_size = geom.Geometry.BytesPerSector ? geom.Geometry.BytesPerSector : 512;
    } else {
        disk.sector_size = 512;
    }

    // Query storage properties (vendor, model, bus type, removable)
    query_disk_properties(hDisk, disk);

    // Query drive layout (GPT / MBR partitions)
    query_disk_layout(hDisk, disk);

    // Probe filesystems for each candidate partition
    for (auto& part : disk.partitions) {
        probe_partition_filesystem(hDisk, part);
    }

    CloseHandle(hDisk);
    return disk;
}

bool DiskEnumerator::query_disk_properties(HANDLE hDisk, DiskInfo& disk) {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    std::vector<uint8_t> buffer(4096);
    DWORD bytesReturned = 0;

    if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesReturned, nullptr)) {
        const auto* desc = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

        disk.is_removable = desc->RemovableMedia != FALSE;

        switch (desc->BusType) {
            case BusTypeUsb:  disk.bus_type = "USB"; break;
            case BusTypeNvme: disk.bus_type = "NVMe"; break;
            case BusTypeSata: disk.bus_type = "SATA"; break;
            case BusTypeScsi: disk.bus_type = "SCSI"; break;
            case BusTypeSas:  disk.bus_type = "SAS"; break;
            case BusTypeVirtual: disk.bus_type = "Virtual"; break;
            default: disk.bus_type = "Other"; break;
        }

        std::string vendor, product, serial;
        if (desc->VendorIdOffset > 0 && desc->VendorIdOffset < bytesReturned) {
            vendor = reinterpret_cast<const char*>(buffer.data() + desc->VendorIdOffset);
            while (!vendor.empty() && (vendor.back() == ' ' || vendor.back() == '\0')) vendor.pop_back();
        }
        if (desc->ProductIdOffset > 0 && desc->ProductIdOffset < bytesReturned) {
            product = reinterpret_cast<const char*>(buffer.data() + desc->ProductIdOffset);
            while (!product.empty() && (product.back() == ' ' || product.back() == '\0')) product.pop_back();
        }
        if (desc->SerialNumberOffset > 0 && desc->SerialNumberOffset < bytesReturned) {
            serial = reinterpret_cast<const char*>(buffer.data() + desc->SerialNumberOffset);
            while (!serial.empty() && (serial.back() == ' ' || serial.back() == '\0')) serial.pop_back();
        }

        disk.friendly_name = vendor.empty() ? product : (vendor + " " + product);
        if (disk.friendly_name.empty()) {
            disk.friendly_name = "Physical Disk " + std::to_string(disk.disk_index);
        }
        disk.serial_number = serial;
        return true;
    }
    return false;
}

bool DiskEnumerator::query_disk_layout(HANDLE hDisk, DiskInfo& disk) {
    std::vector<uint8_t> buffer(16384);
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesReturned, nullptr)) {
        return false;
    }

    const auto* layout = reinterpret_cast<const DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
    disk.has_gpt = (layout->PartitionStyle == PARTITION_STYLE_GPT);

    for (DWORD i = 0; i < layout->PartitionCount; ++i) {
        const auto& pentry = layout->PartitionEntry[i];
        if (pentry.PartitionLength.QuadPart == 0) continue;

        PartitionInfo part{};
        part.disk_index = disk.disk_index;
        part.partition_number = pentry.PartitionNumber;
        part.starting_offset = static_cast<uint64_t>(pentry.StartingOffset.QuadPart);
        part.total_length = static_cast<uint64_t>(pentry.PartitionLength.QuadPart);

        if (pentry.PartitionStyle == PARTITION_STYLE_GPT) {
            part.is_gpt = true;
            part.gpt_type_guid = win_guid_to_guid(pentry.Gpt.PartitionType);
            part.gpt_unique_id = win_guid_to_guid(pentry.Gpt.PartitionId);
            part.kind = PartitionFilter::classify_gpt(part.gpt_type_guid);

            // Convert wchar name
            std::wstring wname(pentry.Gpt.Name);
            int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                std::string u8name(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, u8name.data(), len, nullptr, nullptr);
                part.name = u8name;
            }
            part.is_bootable = (pentry.Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_READ_ONLY) == 0;
        } else if (pentry.PartitionStyle == PARTITION_STYLE_MBR) {
            part.is_gpt = false;
            part.mbr_type = pentry.Mbr.PartitionType;
            part.is_bootable = pentry.Mbr.BootIndicator != FALSE;
            part.kind = PartitionFilter::classify_mbr(part.mbr_type, part.is_bootable);
        }

        // Safety classification: Only safe Linux data partitions are marked mountable
        part.is_data_partition = PartitionFilter::is_safe_data_partition(part.kind);

        disk.partitions.push_back(part);
    }

    return true;
}

void DiskEnumerator::probe_partition_filesystem(HANDLE hDisk, PartitionInfo& part) {
    if (part.total_length < 4096) return;

    // Read first 64KB from partition offset
    constexpr size_t PROBE_SIZE = 65536 + 4096;
    std::vector<uint8_t> probe_buffer(PROBE_SIZE);

    LARGE_INTEGER liOffset;
    liOffset.QuadPart = static_cast<LONGLONG>(part.starting_offset);

    if (SetFilePointerEx(hDisk, liOffset, nullptr, FILE_BEGIN)) {
        DWORD bytesRead = 0;
        if (ReadFile(hDisk, probe_buffer.data(), static_cast<DWORD>(probe_buffer.size()), &bytesRead, nullptr) && bytesRead >= 2048) {
            FilesystemType fs = PartitionFilter::probe_filesystem(probe_buffer.data(), bytesRead);
            part.fs_type = fs;

            // If filesystem is Ext2/3/4, extract superblock metadata (volume label, UUID)
            if (fs == FilesystemType::Ext2 || fs == FilesystemType::Ext3 || fs == FilesystemType::Ext4) {
                const auto* sb = reinterpret_cast<const fs::Ext4Superblock*>(probe_buffer.data() + fs::EXT4_SUPERBLOCK_OFFSET);
                char vol_name[17] = {0};
                std::memcpy(vol_name, sb->s_volume_name, 16);
                part.fs_label = std::string(vol_name);

                // Format UUID
                std::ostringstream ss;
                for (int i = 0; i < 16; ++i) {
                    if (i == 4 || i == 6 || i == 8 || i == 10) ss << "-";
                    ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(sb->s_uuid[i]);
                }
                part.fs_uuid = ss.str();

                // If partition type was Unknown in MBR/GPT, but superblock is valid Ext4, mark as LinuxData!
                if (part.kind == PartitionKind::Unknown) {
                    part.kind = PartitionKind::LinuxData;
                    part.is_data_partition = true;
                }
            }
        }
    }
}

std::vector<PartitionInfo> DiskEnumerator::get_all_mountable_partitions() {
    std::vector<PartitionInfo> mountable;
    auto all_disks = enumerate_all_disks();
    for (const auto& disk : all_disks) {
        for (const auto& part : disk.partitions) {
            // Strictly exclude boot, EFI, swap, and ensure is_data_partition is true
            if (part.is_data_partition && !PartitionFilter::is_excluded_system_partition(part.kind)) {
                mountable.push_back(part);
            }
        }
    }
    return mountable;
}

} // namespace linux2win
