#pragma once

#include <cstdint>

namespace linux2win::fs {

#pragma pack(push, 1)

constexpr uint16_t EXT4_SUPERBLOCK_MAGIC = 0xEF53;
constexpr uint64_t EXT4_SUPERBLOCK_OFFSET = 1024; // 0x400

// Inode Modes
constexpr uint16_t EXT4_S_IFMT   = 0xF000;
constexpr uint16_t EXT4_S_IFSOCK = 0xC000;
constexpr uint16_t EXT4_S_IFLNK  = 0xA000;
constexpr uint16_t EXT4_S_IFREG  = 0x8000;
constexpr uint16_t EXT4_S_IFBLK  = 0x6000;
constexpr uint16_t EXT4_S_IFDIR  = 0x4000;
constexpr uint16_t EXT4_S_IFCHR  = 0x2000;
constexpr uint16_t EXT4_S_IFIFO  = 0x1000;

// Ext4 Inode flags
constexpr uint32_t EXT4_EXTENTS_FL = 0x00080000; // Inode uses extents

// Ext4 Superblock Features
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_GDT_CSUM = 0x0010;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_DIR_NLINK = 0x0020;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE = 0x0040;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_FILETYPE = 0x0002;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS  = 0x0040;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT    = 0x0080;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG  = 0x0200;

// Ext4 Superblock struct (1024 bytes)
struct Ext4Superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;                 // 0xEF53
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    // Dynamic revision fields
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    // 64-bit support
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t  s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint32_t s_last_error_line;
    uint64_t s_last_error_block;
    uint8_t  s_last_error_func[32];
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_blocks;
    uint32_t s_backup_bgs[2];
    uint8_t  s_encrypt_algos[4];
    uint8_t  s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint8_t  s_reserved[98 * 4];
    uint32_t s_checksum;
};

// 32-bit Block Group Descriptor
struct Ext4GroupDesc32 {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
};

// 64-bit Block Group Descriptor extension
struct Ext4GroupDesc64 {
    Ext4GroupDesc32 base;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
};

// Ext4 Inode struct (minimum 128 bytes)
struct Ext4Inode {
    uint16_t i_mode;        // File mode & type
    uint16_t i_uid;         // Low 16 bits of Owner Uid
    uint32_t i_size_lo;     // Size in bytes (low 32 bits)
    uint32_t i_atime;       // Access time
    uint32_t i_ctime;       // Inode Change time
    uint32_t i_mtime;       // Modification time
    uint32_t i_dtime;       // Deletion Time
    uint16_t i_gid;         // Low 16 bits of Group Id
    uint16_t i_links_count; // Links count
    uint32_t i_blocks_lo;   // Blocks count
    uint32_t i_flags;       // File flags (e.g. EXT4_EXTENTS_FL)
    uint32_t i_osd1;        // OS dependent 1
    uint8_t  i_block[60];   // Extents tree or direct/indirect block pointers
    uint32_t i_generation;  // File version (for NFS)
    uint32_t i_file_acl_lo; // File ACL
    uint32_t i_size_high;   // High 32 bits of size (for regular files)
    uint32_t i_obso_faddr;  // Obsoleted fragment address
    uint16_t i_osd2_blocks_high;
    uint16_t i_osd2_file_acl_high;
    uint16_t i_osd2_uid_high;
    uint16_t i_osd2_gid_high;
    uint16_t i_osd2_checksum_lo;
    uint16_t i_osd2_reserved;
    // Extra fields if inode_size > 128
    uint16_t i_extra_isize;
    uint16_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    uint32_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    uint32_t i_projid;
};

// Ext4 Extent Header
struct Ext4ExtentHeader {
    uint16_t eh_magic;      // 0xF30A
    uint16_t eh_entries;    // Number of valid entries following header
    uint16_t eh_max;        // Capacity of store in entries
    uint16_t eh_depth;      // Depth of tree (0 = leaf, >0 = index node)
    uint32_t eh_generation;
};

// Ext4 Extent Index (when depth > 0)
struct Ext4ExtentIdx {
    uint32_t ei_block;      // Index covers logical blocks from this block
    uint32_t ei_leaf_lo;    // Physical block of the next level node (lo 32 bits)
    uint16_t ei_leaf_hi;    // Physical block high 16 bits
    uint16_t ei_unused;
};

// Ext4 Extent Leaf (when depth == 0)
struct Ext4Extent {
    uint32_t ee_block;      // First logical block extent covers
    uint16_t ee_len;        // Number of blocks covered by extent (<= 32768)
    uint16_t ee_start_hi;   // High 16 bits of physical block
    uint32_t ee_start_lo;   // Low 32 bits of physical block
};

// Ext4 Directory Entry 2
struct Ext4DirEntry2 {
    uint32_t inode;         // Inode number
    uint16_t rec_len;       // Directory entry length
    uint8_t  name_len;      // Name length
    uint8_t  file_type;     // File type (1=reg, 2=dir, 7=symlink, etc.)
    char     name[255];     // File name (not necessarily null-terminated)
};

#pragma pack(pop)

} // namespace linux2win::fs
