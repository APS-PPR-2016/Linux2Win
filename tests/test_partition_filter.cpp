#include "linux2win/partition_filter.hpp"
#include <iostream>
#include <cassert>

using namespace linux2win;

void test_guid_parsing() {
    std::string guid_str = "0fc63daf-8483-4772-8e79-3d69d8477de4";
    Guid g = Guid::from_string(guid_str);
    assert(g.data1 == 0x0fc63daf);
    assert(g.data2 == 0x8483);
    assert(g.data3 == 0x4772);
    assert(g.to_string() == guid_str);
    std::cout << "[PASS] test_guid_parsing\n";
}

void test_safe_data_partitions() {
    // Linux Filesystem Data
    PartitionKind k1 = PartitionFilter::classify_gpt(guids::LinuxGenericData);
    assert(k1 == PartitionKind::LinuxData);
    assert(PartitionFilter::is_safe_data_partition(k1));
    assert(!PartitionFilter::is_excluded_system_partition(k1));

    // Linux /home
    PartitionKind k2 = PartitionFilter::classify_gpt(guids::LinuxHome);
    assert(k2 == PartitionKind::LinuxHome);
    assert(PartitionFilter::is_safe_data_partition(k2));

    // Linux Root x86_64
    PartitionKind k3 = PartitionFilter::classify_gpt(guids::LinuxRootX86_64);
    assert(k3 == PartitionKind::LinuxRoot);
    assert(PartitionFilter::is_safe_data_partition(k3));

    // MBR Linux Data (0x83)
    PartitionKind kmbr = PartitionFilter::classify_mbr(0x83, false);
    assert(kmbr == PartitionKind::LinuxData);
    assert(PartitionFilter::is_safe_data_partition(kmbr));

    std::cout << "[PASS] test_safe_data_partitions\n";
}

void test_boot_and_system_exclusions() {
    // EFI System Partition
    PartitionKind k_esp = PartitionFilter::classify_gpt(guids::EfiSystemPartition);
    assert(k_esp == PartitionKind::EfiSystem);
    assert(!PartitionFilter::is_safe_data_partition(k_esp));
    assert(PartitionFilter::is_excluded_system_partition(k_esp));

    // BIOS Boot
    PartitionKind k_bios = PartitionFilter::classify_gpt(guids::BiosBoot);
    assert(k_bios == PartitionKind::BiosBoot);
    assert(!PartitionFilter::is_safe_data_partition(k_bios));
    assert(PartitionFilter::is_excluded_system_partition(k_bios));

    // Linux /boot
    PartitionKind k_boot = PartitionFilter::classify_gpt(guids::LinuxBoot);
    assert(k_boot == PartitionKind::LinuxBoot);
    assert(!PartitionFilter::is_safe_data_partition(k_boot));
    assert(PartitionFilter::is_excluded_system_partition(k_boot));

    // Linux Swap
    PartitionKind k_swap = PartitionFilter::classify_gpt(guids::LinuxSwap);
    assert(k_swap == PartitionKind::LinuxSwap);
    assert(!PartitionFilter::is_safe_data_partition(k_swap));
    assert(PartitionFilter::is_excluded_system_partition(k_swap));

    // MBR Swap (0x82) & EFI (0xEF)
    assert(PartitionFilter::is_excluded_system_partition(PartitionFilter::classify_mbr(0x82, false)));
    assert(PartitionFilter::is_excluded_system_partition(PartitionFilter::classify_mbr(0xEF, false)));

    std::cout << "[PASS] test_boot_and_system_exclusions\n";
}

int main() {
    std::cout << "Running Partition Filter Unit Tests...\n";
    test_guid_parsing();
    test_safe_data_partitions();
    test_boot_and_system_exclusions();
    std::cout << "All Partition Filter Tests PASSED Successfully!\n";
    return 0;
}
