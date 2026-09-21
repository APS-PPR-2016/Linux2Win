#include "linux2win/ext4_reader.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace linux2win::fs {

Ext4Reader::Ext4Reader() = default;

Ext4Reader::~Ext4Reader() {
    unmount();
}

bool Ext4Reader::mount(std::shared_ptr<BlockDevice> device) {
    unmount();

    if (!device || !device->is_open()) return false;
    device_ = device;

    // Read superblock at offset 1024
    if (!device_->read(EXT4_SUPERBLOCK_OFFSET, &sb_, sizeof(sb_))) {
        return false;
    }

    if (sb_.s_magic != EXT4_SUPERBLOCK_MAGIC) {
        return false;
    }

    block_size_ = 1024 << sb_.s_log_block_size;
    if (block_size_ < 1024 || block_size_ > 65536) {
        return false;
    }

    inode_size_ = (sb_.s_rev_level >= 1) ? sb_.s_inode_size : 128;
    if (inode_size_ == 0 || inode_size_ > block_size_) {
        inode_size_ = 128;
    }

    is_64bit_ = (sb_.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0;
    desc_size_ = is_64bit_ ? (sb_.s_desc_size ? sb_.s_desc_size : 64) : 32;
    has_extents_ = (sb_.s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) != 0;

    uint64_t total_blocks_cnt = total_blocks();
    uint32_t bpg = sb_.s_blocks_per_group;
    if (bpg == 0) return false;

    groups_count_ = static_cast<uint32_t>((total_blocks_cnt + bpg - 1) / bpg);

    mounted_ = true;
    return true;
}

void Ext4Reader::unmount() {
    mounted_ = false;
    device_.reset();
    std::memset(&sb_, 0, sizeof(sb_));
}

uint64_t Ext4Reader::total_blocks() const noexcept {
    uint64_t cnt = sb_.s_blocks_count_lo;
    if (is_64bit_) {
        cnt |= (static_cast<uint64_t>(sb_.s_blocks_count_hi) << 32);
    }
    return cnt;
}

std::string Ext4Reader::volume_name() const {
    char name[17] = {0};
    std::memcpy(name, sb_.s_volume_name, 16);
    return std::string(name);
}

std::string Ext4Reader::volume_uuid() const {
    std::ostringstream ss;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) ss << "-";
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(sb_.s_uuid[i]);
    }
    return ss.str();
}

uint64_t Ext4Reader::get_group_descriptor_block(uint32_t group_idx) const {
    // Superblock is at block 1 for 1KB block size, or block 0 for >1KB block size
    uint64_t gdt_start_block = (block_size_ == 1024) ? 2 : 1;
    uint64_t byte_offset = static_cast<uint64_t>(group_idx) * desc_size_;
    return gdt_start_block + (byte_offset / block_size_);
}

bool Ext4Reader::read_group_descriptor(uint32_t group_idx, Ext4GroupDesc64& out_desc) {
    if (!mounted_ || group_idx >= groups_count_) return false;

    uint64_t gdt_start_byte = ((block_size_ == 1024) ? 2 : 1) * static_cast<uint64_t>(block_size_);
    uint64_t desc_byte_offset = gdt_start_byte + (static_cast<uint64_t>(group_idx) * desc_size_);

    if (is_64bit_ && desc_size_ >= sizeof(Ext4GroupDesc64)) {
        return device_->read(desc_byte_offset, &out_desc, sizeof(Ext4GroupDesc64));
    } else {
        Ext4GroupDesc32 desc32{};
        if (!device_->read(desc_byte_offset, &desc32, sizeof(Ext4GroupDesc32))) {
            return false;
        }
        std::memset(&out_desc, 0, sizeof(out_desc));
        out_desc.base = desc32;
        return true;
    }
}

bool Ext4Reader::read_inode(uint32_t inode_num, Ext4Inode& out_inode) {
    if (!mounted_ || inode_num == 0 || inode_num > sb_.s_inodes_count) return false;

    // Inodes are 1-indexed
    uint32_t inode_idx = inode_num - 1;
    uint32_t group_idx = inode_idx / sb_.s_inodes_per_group;
    uint32_t index_in_group = inode_idx % sb_.s_inodes_per_group;

    Ext4GroupDesc64 gd{};
    if (!read_group_descriptor(group_idx, gd)) {
        return false;
    }

    uint64_t itable_block = gd.base.bg_inode_table_lo;
    if (is_64bit_) {
        itable_block |= (static_cast<uint64_t>(gd.bg_inode_table_hi) << 32);
    }

    uint64_t inode_byte_offset = (itable_block * static_cast<uint64_t>(block_size_)) + (static_cast<uint64_t>(index_in_group) * inode_size_);

    std::vector<uint8_t> buffer(inode_size_);
    if (!device_->read(inode_byte_offset, buffer.data(), inode_size_)) {
        return false;
    }

    std::memset(&out_inode, 0, sizeof(out_inode));
    size_t copy_size = (sizeof(out_inode) < inode_size_) ? sizeof(out_inode) : inode_size_;
    std::memcpy(&out_inode, buffer.data(), copy_size);

    return true;
}

bool Ext4Reader::stat_inode(uint32_t inode_num, FileStat& out_stat) {
    Ext4Inode inode{};
    if (!read_inode(inode_num, inode)) return false;

    out_stat.inode_num = inode_num;
    out_stat.mode = inode.i_mode;
    out_stat.uid = inode.i_uid | (static_cast<uint32_t>(inode.i_osd2_uid_high) << 16);
    out_stat.gid = inode.i_gid | (static_cast<uint32_t>(inode.i_osd2_gid_high) << 16);
    out_stat.atime = inode.i_atime;
    out_stat.mtime = inode.i_mtime;
    out_stat.ctime = inode.i_ctime;
    out_stat.crtime = (inode_size_ > 128) ? inode.i_crtime : inode.i_mtime;

    uint16_t type = inode.i_mode & EXT4_S_IFMT;
    out_stat.is_directory = (type == EXT4_S_IFDIR);
    out_stat.is_regular   = (type == EXT4_S_IFREG);
    out_stat.is_symlink   = (type == EXT4_S_IFLNK);

    uint64_t size = inode.i_size_lo;
    if (out_stat.is_regular) {
        size |= (static_cast<uint64_t>(inode.i_size_high) << 32);
    }
    out_stat.size = size;

    if (out_stat.is_symlink) {
        read_symlink_target(inode_num, out_stat.symlink_target);
    }

    return true;
}

bool Ext4Reader::stat_path(const std::string& path, FileStat& out_stat) {
    auto inum = lookup_path(path);
    if (!inum.has_value()) return false;
    return stat_inode(inum.value(), out_stat);
}

bool Ext4Reader::resolve_extent_block(const Ext4Inode& inode, uint32_t logical_block, uint64_t& physical_block) {
    const auto* eh = reinterpret_cast<const Ext4ExtentHeader*>(inode.i_block);
    if (eh->eh_magic != 0xF30A) {
        return false;
    }

    if (eh->eh_depth == 0) {
        const auto* ex = reinterpret_cast<const Ext4Extent*>(inode.i_block + sizeof(Ext4ExtentHeader));
        for (uint16_t i = 0; i < eh->eh_entries; ++i) {
            uint32_t ee_block = ex[i].ee_block;
            uint16_t ee_len = (ex[i].ee_len <= 32768) ? ex[i].ee_len : (ex[i].ee_len - 32768);
            if (logical_block >= ee_block && logical_block < ee_block + ee_len) {
                uint64_t start = ex[i].ee_start_lo | (static_cast<uint64_t>(ex[i].ee_start_hi) << 32);
                physical_block = start + (logical_block - ee_block);
                return true;
            }
        }
        return false;
    } else {
        const auto* idx = reinterpret_cast<const Ext4ExtentIdx*>(inode.i_block + sizeof(Ext4ExtentHeader));
        int chosen_idx = -1;
        for (uint16_t i = 0; i < eh->eh_entries; ++i) {
            if (logical_block >= idx[i].ei_block) {
                chosen_idx = i;
            } else {
                break;
            }
        }
        if (chosen_idx >= 0) {
            uint64_t next_block = idx[chosen_idx].ei_leaf_lo | (static_cast<uint64_t>(idx[chosen_idx].ei_leaf_hi) << 32);
            return parse_extent_node(next_block, logical_block, physical_block);
        }
        return false;
    }
}

bool Ext4Reader::parse_extent_node(uint64_t block_num, uint32_t logical_block, uint64_t& physical_block) {
    std::vector<uint8_t> block_data(block_size_);
    if (!device_->read_block(block_num, block_size_, block_data.data())) {
        return false;
    }

    const auto* eh = reinterpret_cast<const Ext4ExtentHeader*>(block_data.data());
    if (eh->eh_magic != 0xF30A) return false;

    if (eh->eh_depth == 0) {
        const auto* ex = reinterpret_cast<const Ext4Extent*>(block_data.data() + sizeof(Ext4ExtentHeader));
        for (uint16_t i = 0; i < eh->eh_entries; ++i) {
            uint32_t ee_block = ex[i].ee_block;
            uint16_t ee_len = (ex[i].ee_len <= 32768) ? ex[i].ee_len : (ex[i].ee_len - 32768);
            if (logical_block >= ee_block && logical_block < ee_block + ee_len) {
                uint64_t start = ex[i].ee_start_lo | (static_cast<uint64_t>(ex[i].ee_start_hi) << 32);
                physical_block = start + (logical_block - ee_block);
                return true;
            }
        }
        return false;
    } else {
        const auto* idx = reinterpret_cast<const Ext4ExtentIdx*>(block_data.data() + sizeof(Ext4ExtentHeader));
        int chosen_idx = -1;
        for (uint16_t i = 0; i < eh->eh_entries; ++i) {
            if (logical_block >= idx[i].ei_block) {
                chosen_idx = i;
            } else {
                break;
            }
        }
        if (chosen_idx >= 0) {
            uint64_t next_block = idx[chosen_idx].ei_leaf_lo | (static_cast<uint64_t>(idx[chosen_idx].ei_leaf_hi) << 32);
            return parse_extent_node(next_block, logical_block, physical_block);
        }
        return false;
    }
}

bool Ext4Reader::read_indirect_block(const Ext4Inode& inode, uint32_t logical_block, uint64_t& physical_block) {
    // Ext2/3 direct and indirect blocks
    const uint32_t* direct = reinterpret_cast<const uint32_t*>(inode.i_block);

    // 0..11: Direct blocks
    if (logical_block < 12) {
        physical_block = direct[logical_block];
        return physical_block != 0;
    }

    uint32_t ptrs_per_block = block_size_ / 4;
    logical_block -= 12;

    // Singly indirect block (block 12)
    if (logical_block < ptrs_per_block) {
        uint32_t ind_block = direct[12];
        if (ind_block == 0) return false;
        std::vector<uint32_t> ind_buf(ptrs_per_block);
        if (!device_->read_block(ind_block, block_size_, ind_buf.data())) return false;
        physical_block = ind_buf[logical_block];
        return physical_block != 0;
    }

    logical_block -= ptrs_per_block;

    // Doubly indirect block (block 13)
    uint32_t ptrs_sq = ptrs_per_block * ptrs_per_block;
    if (logical_block < ptrs_sq) {
        uint32_t dind_block = direct[13];
        if (dind_block == 0) return false;
        std::vector<uint32_t> dind_buf(ptrs_per_block);
        if (!device_->read_block(dind_block, block_size_, dind_buf.data())) return false;

        uint32_t idx1 = logical_block / ptrs_per_block;
        uint32_t idx2 = logical_block % ptrs_per_block;

        uint32_t ind_block = dind_buf[idx1];
        if (ind_block == 0) return false;
        std::vector<uint32_t> ind_buf(ptrs_per_block);
        if (!device_->read_block(ind_block, block_size_, ind_buf.data())) return false;

        physical_block = ind_buf[idx2];
        return physical_block != 0;
    }

    return false;
}

bool Ext4Reader::read_file_data(uint32_t inode_num, uint64_t offset, void* buffer, size_t size, size_t* bytes_read) {
    if (bytes_read) *bytes_read = 0;
    if (!mounted_ || !buffer || size == 0) return false;

    Ext4Inode inode{};
    if (!read_inode(inode_num, inode)) return false;

    uint64_t file_size = inode.i_size_lo;
    if ((inode.i_mode & EXT4_S_IFMT) == EXT4_S_IFREG) {
        file_size |= (static_cast<uint64_t>(inode.i_size_high) << 32);
    }

    if (offset >= file_size) {
        return true; // EOF, read 0 bytes
    }

    size_t actual_size = size;
    if (offset + actual_size > file_size) {
        actual_size = static_cast<size_t>(file_size - offset);
    }

    bool uses_extents = (inode.i_flags & EXT4_EXTENTS_FL) != 0;

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    size_t remaining = actual_size;
    uint64_t curr_offset = offset;
    std::vector<uint8_t> block_buf(block_size_);

    while (remaining > 0) {
        uint32_t logical_block = static_cast<uint32_t>(curr_offset / block_size_);
        uint32_t block_offset = static_cast<uint32_t>(curr_offset % block_size_);
        uint32_t to_copy = std::min(static_cast<uint32_t>(remaining), block_size_ - block_offset);

        uint64_t phys_block = 0;
        bool has_block = false;
        if (uses_extents) {
            has_block = resolve_extent_block(inode, logical_block, phys_block);
        } else {
            has_block = read_indirect_block(inode, logical_block, phys_block);
        }

        if (has_block && phys_block != 0) {
            if (!device_->read_block(phys_block, block_size_, block_buf.data())) {
                return false;
            }
            std::memcpy(dst, block_buf.data() + block_offset, to_copy);
        } else {
            // Sparse file block (zeros)
            std::memset(dst, 0, to_copy);
        }

        dst += to_copy;
        curr_offset += to_copy;
        remaining -= to_copy;
    }

    if (bytes_read) *bytes_read = actual_size;
    return true;
}

bool Ext4Reader::read_symlink_target(uint32_t inode_num, std::string& out_target) {
    out_target.clear();
    Ext4Inode inode{};
    if (!read_inode(inode_num, inode)) return false;

    uint64_t size = inode.i_size_lo;
    if (size == 0) return true;

    // Fast symlink: target is stored directly in i_block if size < 60
    if (size < sizeof(inode.i_block)) {
        char buf[61] = {0};
        std::memcpy(buf, inode.i_block, size);
        out_target = std::string(buf, size);
        return true;
    }

    // Normal block-backed symlink
    std::vector<char> buf(size + 1, 0);
    size_t read_bytes = 0;
    if (read_file_data(inode_num, 0, buf.data(), size, &read_bytes)) {
        out_target = std::string(buf.data(), read_bytes);
        return true;
    }

    return false;
}

bool Ext4Reader::list_directory(uint32_t dir_inode_num, std::vector<DirEntryInfo>& out_entries) {
    out_entries.clear();
    if (!mounted_) return false;

    Ext4Inode inode{};
    if (!read_inode(dir_inode_num, inode)) return false;

    if ((inode.i_mode & EXT4_S_IFMT) != EXT4_S_IFDIR) return false;

    uint64_t dir_size = inode.i_size_lo;
    if (dir_size == 0) return true;

    std::vector<uint8_t> dir_content(dir_size);
    size_t bytes_read = 0;
    if (!read_file_data(dir_inode_num, 0, dir_content.data(), dir_size, &bytes_read)) {
        return false;
    }

    size_t offset = 0;
    while (offset + 8 <= bytes_read) {
        const auto* entry = reinterpret_cast<const Ext4DirEntry2*>(dir_content.data() + offset);
        if (entry->rec_len == 0) break; // Corrupt entry guard

        if (entry->inode != 0 && entry->name_len > 0) {
            std::string name(entry->name, entry->name_len);
            if (name != "." && name != "..") {
                DirEntryInfo info;
                info.inode = entry->inode;
                info.name = name;
                info.file_type = entry->file_type;
                info.is_directory = (entry->file_type == 2);
                info.is_symlink = (entry->file_type == 7);

                // If file_type is not populated (old ext2 format), stat the inode
                if (info.file_type == 0) {
                    Ext4Inode child_inode{};
                    if (read_inode(entry->inode, child_inode)) {
                        uint16_t child_type = child_inode.i_mode & EXT4_S_IFMT;
                        info.is_directory = (child_type == EXT4_S_IFDIR);
                        info.is_symlink = (child_type == EXT4_S_IFLNK);
                    }
                }

                out_entries.push_back(info);
            }
        }

        offset += entry->rec_len;
    }

    return true;
}

std::optional<uint32_t> Ext4Reader::lookup_path(const std::string& path) {
    if (!mounted_) return std::nullopt;

    // Root inode is inode 2 in ext2/3/4
    uint32_t curr_inode = 2;

    if (path.empty() || path == "/" || path == "\\" || path == ".") {
        return curr_inode;
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    for (const auto& part : parts) {
        std::vector<DirEntryInfo> entries;
        if (!list_directory(curr_inode, entries)) {
            return std::nullopt;
        }

        bool found = false;
        for (const auto& e : entries) {
            if (e.name == part) {
                curr_inode = e.inode;
                found = true;
                break;
            }
        }

        if (!found) {
            return std::nullopt;
        }
    }

    return curr_inode;
}

bool Ext4Reader::list_directory_path(const std::string& path, std::vector<DirEntryInfo>& out_entries) {
    auto inum = lookup_path(path);
    if (!inum.has_value()) return false;
    return list_directory(inum.value(), out_entries);
}

bool Ext4Reader::read_file_by_path(const std::string& path, uint64_t offset, void* buffer, size_t size, size_t* bytes_read) {
    auto inum = lookup_path(path);
    if (!inum.has_value()) return false;
    return read_file_data(inum.value(), offset, buffer, size, bytes_read);
}

} // namespace linux2win::fs
