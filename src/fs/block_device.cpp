#include "linux2win/block_device.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace linux2win {

BlockDevice::BlockDevice() = default;

BlockDevice::~BlockDevice() {
    close();
}

BlockDevice::BlockDevice(BlockDevice&& other) noexcept {
    handle_ = other.handle_;
    device_path_ = std::move(other.device_path_);
    partition_offset_ = other.partition_offset_;
    partition_size_ = other.partition_size_;
    sector_size_ = other.sector_size_;

    other.handle_ = INVALID_HANDLE_VALUE;
}

BlockDevice& BlockDevice::operator=(BlockDevice&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        device_path_ = std::move(other.device_path_);
        partition_offset_ = other.partition_offset_;
        partition_size_ = other.partition_size_;
        sector_size_ = other.sector_size_;

        other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool BlockDevice::open_read_only(const std::string& device_path, uint64_t partition_offset, uint64_t partition_size) {
    close();

    std::wstring wpath(device_path.begin(), device_path.end());

    // STRICTLY GENERIC_READ and FILE_SHARE_READ | FILE_SHARE_WRITE
    handle_ = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    // If NO_BUFFERING fails, fallback to standard buffered read
    if (handle_ == INVALID_HANDLE_VALUE) {
        handle_ = CreateFileW(
            wpath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
    }

    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    device_path_ = device_path;
    partition_offset_ = partition_offset;
    partition_size_ = partition_size;
    sector_size_ = 512;

    return true;
}

void BlockDevice::close() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool BlockDevice::read(uint64_t offset, void* buffer, size_t size) {
    if (!is_open() || !buffer || size == 0) return false;

    // Check bounds if partition size is specified
    if (partition_size_ > 0 && offset + size > partition_size_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(io_mutex_);

    uint64_t absolute_offset = partition_offset_ + offset;

    // Direct sector alignment calculation
    uint64_t aligned_start = (absolute_offset / sector_size_) * sector_size_;
    uint64_t offset_in_aligned = absolute_offset - aligned_start;
    uint64_t aligned_end = ((absolute_offset + size + sector_size_ - 1) / sector_size_) * sector_size_;
    size_t aligned_size = static_cast<size_t>(aligned_end - aligned_start);

    std::vector<uint8_t> aligned_buffer(aligned_size);

    LARGE_INTEGER liOffset;
    liOffset.QuadPart = static_cast<LONGLONG>(aligned_start);

    if (!SetFilePointerEx(handle_, liOffset, nullptr, FILE_BEGIN)) {
        return false;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(handle_, aligned_buffer.data(), static_cast<DWORD>(aligned_size), &bytesRead, nullptr)) {
        return false;
    }

    if (bytesRead < offset_in_aligned + size) {
        return false;
    }

    std::memcpy(buffer, aligned_buffer.data() + offset_in_aligned, size);
    return true;
}

bool BlockDevice::read_block(uint64_t block_index, uint32_t block_size, void* buffer) {
    uint64_t byte_offset = block_index * static_cast<uint64_t>(block_size);
    return read(byte_offset, buffer, block_size);
}

} // namespace linux2win
