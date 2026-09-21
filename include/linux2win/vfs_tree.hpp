#pragma once

#include "linux2win/types.hpp"
#include "linux2win/ext4_reader.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace linux2win::vfs {

struct VfsNode {
    std::string name;
    std::string relative_path; // Windows-style relative path from mount root, e.g. "etc\\passwd"
    uint32_t inode_num{0};
    uint64_t file_size{0};
    uint32_t file_attributes{FILE_ATTRIBUTE_READONLY};
    FILETIME creation_time{};
    FILETIME last_access_time{};
    FILETIME last_write_time{};
    bool is_directory{false};
    bool is_symlink{false};
    bool children_loaded{false};
    std::vector<std::shared_ptr<VfsNode>> children;

    static FILETIME unix_epoch_to_filetime(uint32_t unix_time);
};

class VfsTree {
public:
    VfsTree(std::shared_ptr<fs::Ext4Reader> reader);
    ~VfsTree();

    std::shared_ptr<VfsNode> get_root();
    std::shared_ptr<VfsNode> find_node(const std::wstring& relative_path_w);
    std::shared_ptr<VfsNode> find_node(const std::string& relative_path);

    bool ensure_children_loaded(std::shared_ptr<VfsNode> parent);

    std::shared_ptr<fs::Ext4Reader> reader() const { return reader_; }

private:
    std::shared_ptr<fs::Ext4Reader> reader_;
    std::shared_ptr<VfsNode> root_;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> node_cache_;
    std::mutex tree_mutex_;

    std::string normalize_path(const std::string& path);
};

} // namespace linux2win::vfs
