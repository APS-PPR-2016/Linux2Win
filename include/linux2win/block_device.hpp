#pragma once

#include "linux2win/types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

namespace linux2win {

class BlockDevice {
public:
    BlockDevice();
    ~BlockDevice();

    // Prevent copying
    BlockDevice(const BlockDevice&) = delete;
    BlockDevice& operator=(const BlockDevice&) = delete;

    // Move semantics
    BlockDevice(BlockDevice&& other) noexcept;
    BlockDevice& operator=(BlockDevice&& other) noexcept;

    // Open disk or partition strictly in GENERIC_READ mode
    bool open_read_only(const std::string& device_path, uint64_t partition_offset = 0, uint64_t partition_size = 0);
    void close();

    bool is_open() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }
    uint64_t size() const noexcept { return partition_size_; }
    uint64_t offset() const noexcept { return partition_offset_; }
    uint32_t sector_size() const noexcept { return sector_size_; }

    // Read bytes at absolute or partition-relative offset (strictly read-only)
    bool read(uint64_t offset, void* buffer, size_t size);

    // Read exact block with caching
    bool read_block(uint64_t block_index, uint32_t block_size, void* buffer);

    // Explicitly reject any write attempts (Safety guarantee)
    bool write(uint64_t offset, const void* buffer, size_t size) {
        // Strictly prohibited
        SetLastError(ERROR_ACCESS_DENIED);
        return false;
    }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
    std::string device_path_;
    uint64_t partition_offset_{0};
    uint64_t partition_size_{0};
    uint32_t sector_size_{512};
    std::mutex io_mutex_;
};

} // namespace linux2win
