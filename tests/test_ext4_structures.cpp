#include "linux2win/ext4_types.hpp"
#include "linux2win/partition_filter.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

using namespace linux2win;
using namespace linux2win::fs;

void test_superblock_probing() {
    std::vector<uint8_t> buffer(4096, 0);

    // Write magic 0xEF53 at offset 1024
    auto* sb = reinterpret_cast<Ext4Superblock*>(buffer.data() + EXT4_SUPERBLOCK_OFFSET);
    sb->s_magic = EXT4_SUPERBLOCK_MAGIC;
    sb->s_log_block_size = 2; // 4096 block size (1024 << 2)
    sb->s_inodes_count = 1000;
    sb->s_blocks_count_lo = 100000;
    sb->s_feature_incompat = EXT4_FEATURE_INCOMPAT_EXTENTS | EXT4_FEATURE_INCOMPAT_64BIT;

    FilesystemType fs_type = PartitionFilter::probe_filesystem(buffer.data(), buffer.size());
    assert(fs_type == FilesystemType::Ext4);
    assert(PartitionFilter::fs_type_to_string(fs_type) == "Ext4");

    std::cout << "[PASS] test_superblock_probing\n";
}

void test_extent_header_structure() {
    Ext4ExtentHeader eh{};
    eh.eh_magic = 0xF30A;
    eh.eh_entries = 2;
    eh.eh_max = 4;
    eh.eh_depth = 0; // Leaf

    assert(sizeof(Ext4ExtentHeader) == 12);
    assert(sizeof(Ext4Extent) == 12);
    assert(sizeof(Ext4ExtentIdx) == 12);

    Ext4Extent ex[2]{};
    ex[0].ee_block = 0;
    ex[0].ee_len = 10;
    ex[0].ee_start_lo = 100;
    ex[0].ee_start_hi = 0;

    ex[1].ee_block = 10;
    ex[1].ee_len = 20;
    ex[1].ee_start_lo = 200;
    ex[1].ee_start_hi = 0;

    assert(ex[0].ee_len == 10);
    assert(ex[1].ee_len == 20);

    std::cout << "[PASS] test_extent_header_structure\n";
}

void test_directory_entry_layout() {
    assert(sizeof(Ext4DirEntry2) == 263); // 4 + 2 + 1 + 1 + 255 = 263 bytes unpadded struct

    std::vector<uint8_t> dir_buf(256, 0);
    auto* de = reinterpret_cast<Ext4DirEntry2*>(dir_buf.data());
    de->inode = 12345;
    de->rec_len = 24;
    de->name_len = 6;
    de->file_type = 1; // regular file
    std::memcpy(de->name, "config", 6);

    assert(de->inode == 12345);
    assert(std::string(de->name, de->name_len) == "config");

    std::cout << "[PASS] test_directory_entry_layout\n";
}

int main() {
    std::cout << "Running Ext4 Structure Unit Tests...\n";
    test_superblock_probing();
    test_extent_header_structure();
    test_directory_entry_layout();
    std::cout << "All Ext4 Structure Tests PASSED Successfully!\n";
    return 0;
}
