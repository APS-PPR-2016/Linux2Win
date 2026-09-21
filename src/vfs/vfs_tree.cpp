#include "linux2win/vfs_tree.hpp"
#include <algorithm>
#include <iostream>

namespace linux2win::vfs {

FILETIME VfsNode::unix_epoch_to_filetime(uint32_t unix_time) {
    // 100-nanosecond intervals between January 1, 1601 and January 1, 1970
    constexpr uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    uint64_t ft_val = (static_cast<uint64_t>(unix_time) * 10000000ULL) + EPOCH_DIFFERENCE;

    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(ft_val & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(ft_val >> 32);
    return ft;
}

VfsTree::VfsTree(std::shared_ptr<fs::Ext4Reader> reader) : reader_(reader) {}

VfsTree::~VfsTree() = default;

std::string VfsTree::normalize_path(const std::string& path) {
    std::string norm;
    for (char c : path) {
        if (c == '/') {
            if (!norm.empty() && norm.back() != '\\') {
                norm += '\\';
            }
        } else if (c == '\\') {
            if (!norm.empty() && norm.back() != '\\') {
                norm += '\\';
            }
        } else {
            norm += c;
        }
    }
    // Remove leading and trailing backslashes
    while (!norm.empty() && (norm.front() == '\\')) norm.erase(norm.begin());
    while (!norm.empty() && (norm.back() == '\\')) norm.pop_back();
    return norm;
}

std::shared_ptr<VfsNode> VfsTree::get_root() {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    if (!root_ && reader_ && reader_->is_mounted()) {
        fs::FileStat root_stat{};
        if (reader_->stat_inode(2, root_stat)) {
            root_ = std::make_shared<VfsNode>();
            root_->name = "";
            root_->relative_path = "";
            root_->inode_num = 2;
            root_->file_size = root_stat.size;
            root_->is_directory = true;
            root_->file_attributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
            root_->creation_time = VfsNode::unix_epoch_to_filetime(root_stat.crtime);
            root_->last_access_time = VfsNode::unix_epoch_to_filetime(root_stat.atime);
            root_->last_write_time = VfsNode::unix_epoch_to_filetime(root_stat.mtime);
            root_->children_loaded = false;

            node_cache_[""] = root_;
        }
    }
    return root_;
}

bool VfsTree::ensure_children_loaded(std::shared_ptr<VfsNode> parent) {
    if (!parent || !parent->is_directory || !reader_) return false;
    if (parent->children_loaded) return true;

    std::vector<fs::DirEntryInfo> entries;
    if (!reader_->list_directory(parent->inode_num, entries)) {
        return false;
    }

    parent->children.clear();
    for (const auto& entry : entries) {
        fs::FileStat st{};
        if (reader_->stat_inode(entry.inode, st)) {
            auto child = std::make_shared<VfsNode>();
            child->name = entry.name;
            child->relative_path = parent->relative_path.empty() ? entry.name : (parent->relative_path + "\\" + entry.name);
            child->inode_num = entry.inode;
            child->file_size = st.size;
            child->is_directory = st.is_directory;
            child->is_symlink = st.is_symlink;
            child->file_attributes = FILE_ATTRIBUTE_READONLY | (st.is_directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL);
            child->creation_time = VfsNode::unix_epoch_to_filetime(st.crtime);
            child->last_access_time = VfsNode::unix_epoch_to_filetime(st.atime);
            child->last_write_time = VfsNode::unix_epoch_to_filetime(st.mtime);
            child->children_loaded = false;

            parent->children.push_back(child);
            node_cache_[normalize_path(child->relative_path)] = child;
        }
    }

    parent->children_loaded = true;
    return true;
}

std::shared_ptr<VfsNode> VfsTree::find_node(const std::string& relative_path) {
    std::lock_guard<std::mutex> lock(tree_mutex_);

    std::string norm = normalize_path(relative_path);
    if (norm.empty()) {
        return get_root();
    }

    auto it = node_cache_.find(norm);
    if (it != node_cache_.end()) {
        return it->second;
    }

    // Traverse component by component
    auto current = get_root();
    if (!current) return nullptr;

    std::vector<std::string> parts;
    size_t start = 0;
    while (start < norm.length()) {
        size_t end = norm.find('\\', start);
        if (end == std::string::npos) {
            parts.push_back(norm.substr(start));
            break;
        }
        parts.push_back(norm.substr(start, end - start));
        start = end + 1;
    }

    for (const auto& part : parts) {
        if (!current->is_directory) return nullptr;
        ensure_children_loaded(current);

        std::shared_ptr<VfsNode> next_node = nullptr;
        for (const auto& child : current->children) {
            // Case-insensitive comparison for Windows file compatibility
            if (_stricmp(child->name.c_str(), part.c_str()) == 0) {
                next_node = child;
                break;
            }
        }

        if (!next_node) {
            return nullptr;
        }
        current = next_node;
    }

    return current;
}

std::shared_ptr<VfsNode> VfsTree::find_node(const std::wstring& relative_path_w) {
    std::string path_str(relative_path_w.begin(), relative_path_w.end());
    return find_node(path_str);
}

} // namespace linux2win::vfs
