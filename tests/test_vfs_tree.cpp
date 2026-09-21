#include "linux2win/vfs_tree.hpp"
#include <iostream>
#include <cassert>

using namespace linux2win::vfs;

void test_unix_epoch_conversion() {
    // 0 Unix epoch (1970-01-01 00:00:00 UTC) -> 116444736000000000 100ns
    FILETIME ft0 = VfsNode::unix_epoch_to_filetime(0);
    uint64_t val0 = (static_cast<uint64_t>(ft0.dwHighDateTime) << 32) | ft0.dwLowDateTime;
    assert(val0 == 116444736000000000ULL);

    // 1 second later
    FILETIME ft1 = VfsNode::unix_epoch_to_filetime(1);
    uint64_t val1 = (static_cast<uint64_t>(ft1.dwHighDateTime) << 32) | ft1.dwLowDateTime;
    assert(val1 == 116444736000000000ULL + 10000000ULL);

    std::cout << "[PASS] test_unix_epoch_conversion\n";
}

void test_vfs_node_attributes() {
    VfsNode node;
    node.name = "syslog.log";
    node.relative_path = "var\\log\\syslog.log";
    node.file_size = 1048576;
    node.is_directory = false;
    node.file_attributes = FILE_ATTRIBUTE_READONLY;

    assert((node.file_attributes & FILE_ATTRIBUTE_READONLY) != 0);
    assert(!node.is_directory);
    assert(node.file_size == 1048576);

    std::cout << "[PASS] test_vfs_node_attributes\n";
}

int main() {
    std::cout << "Running VFS Tree Unit Tests...\n";
    test_unix_epoch_conversion();
    test_vfs_node_attributes();
    std::cout << "All VFS Tree Tests PASSED Successfully!\n";
    return 0;
}
