#pragma once

#include "linux2win/block_device.hpp"
#include "linux2win/ext4_types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace linux2win::fs {

struct DirEntryInfo {
    uint32_t inode{0};
    std::string name;
    uint8_t file_type{0}; // 1=Regular, 2=Directory, 7=Symlink
    bool is_directory{false};
    bool is_symlink{false};
};

struct FileStat {
    uint32_t inode_num{0};
    uint64_t size{0};
    uint32_t mode{0};
    uint32_t uid{0};
    uint32_t gid{0};
    uint32_t atime{0};
    uint32_t mtime{0};
    uint32_t ctime{0};
    uint32_t crtime{0};
    bool is_directory{false};
    bool is_regular{false};
    bool is_symlink{false};
    std::string symlink_target;
};

class Ext4Reader {
public:
    Ext4Reader();
    ~Ext4Reader();

    // Mounts an ext2/3/4 filesystem from a block device
    bool mount(std::shared_ptr<BlockDevice> device);
    void unmount();

    bool is_mounted() const noexcept { return mounted_; }

    const Ext4Superblock& superblock() const noexcept { return sb_; }
    uint32_t block_size() const noexcept { return block_size_; }
    uint32_t inode_size() const noexcept { return inode_size_; }
    uint32_t total_inodes() const noexcept { return sb_.s_inodes_count; }
    uint64_t total_blocks() const noexcept;
    std::string volume_name() const;
    std::string volume_uuid() const;

    // Inode operations
    bool read_inode(uint32_t inode_num, Ext4Inode& out_inode);
    bool stat_inode(uint32_t inode_num, FileStat& out_stat);
    bool stat_path(const std::string& path, FileStat& out_stat);

    // Directory operations
    bool list_directory(uint32_t dir_inode_num, std::vector<DirEntryInfo>& out_entries);
    bool list_directory_path(const std::string& path, std::vector<DirEntryInfo>& out_entries);
    std::optional<uint32_t> lookup_path(const std::string& path);

    // File content operations (strictly read-only)
    bool read_file_data(uint32_t inode_num, uint64_t offset, void* buffer, size_t size, size_t* bytes_read = nullptr);
    bool read_file_by_path(const std::string& path, uint64_t offset, void* buffer, size_t size, size_t* bytes_read = nullptr);
    bool read_symlink_target(uint32_t inode_num, std::string& out_target);

private:
    uint64_t get_group_descriptor_block(uint32_t group_idx) const;
    bool read_group_descriptor(uint32_t group_idx, Ext4GroupDesc64& out_desc);
    bool resolve_extent_block(const Ext4Inode& inode, uint32_t logical_block, uint64_t& physical_block);
    bool parse_extent_node(uint64_t block_num, uint32_t logical_block, uint64_t& physical_block);
    bool read_indirect_block(const Ext4Inode& inode, uint32_t logical_block, uint64_t& physical_block);

    std::shared_ptr<BlockDevice> device_;
    Ext4Superblock sb_{};
    uint32_t block_size_{4096};
    uint32_t inode_size_{256};
    uint32_t groups_count_{0};
    uint16_t desc_size_{32};
    bool is_64bit_{false};
    bool has_extents_{false};
    bool mounted_{false};
};

} // namespace linux2win::fs
