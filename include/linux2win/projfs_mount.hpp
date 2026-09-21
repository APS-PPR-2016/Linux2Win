#pragma once

#include "linux2win/types.hpp"
#include "linux2win/vfs_tree.hpp"
#include <string>
#include <memory>
#include <atomic>

#include <projectedfslib.h>

namespace linux2win::vfs {

class ProjFSMount {
public:
    ProjFSMount(std::shared_ptr<VfsTree> tree, const std::wstring& target_root_path);
    ~ProjFSMount();

    // Prevent copy
    ProjFSMount(const ProjFSMount&) = delete;
    ProjFSMount& operator=(const ProjFSMount&) = delete;

    // Initialize root folder and start virtualization instance
    bool start();
    void stop();

    bool is_active() const noexcept { return active_; }
    const std::wstring& target_root_path() const noexcept { return target_root_path_; }

    // ProjFS Callbacks
    static HRESULT CALLBACK StartDirEnumCallback(
        const PRJ_CALLBACK_DATA* callbackData,
        const GUID* enumerationId
    );

    static HRESULT CALLBACK EndDirEnumCallback(
        const PRJ_CALLBACK_DATA* callbackData,
        const GUID* enumerationId
    );

    static HRESULT CALLBACK GetDirEnumCallback(
        const PRJ_CALLBACK_DATA* callbackData,
        const GUID* enumerationId,
        PCWSTR searchExpression,
        PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle
    );

    static HRESULT CALLBACK GetPlaceholderInfoCallback(
        const PRJ_CALLBACK_DATA* callbackData
    );

    static HRESULT CALLBACK GetFileDataCallback(
        const PRJ_CALLBACK_DATA* callbackData,
        UINT64 byteOffset,
        UINT32 length
    );

    static void CALLBACK CancelCommandCallback(
        const PRJ_CALLBACK_DATA* callbackData
    );

    static HRESULT CALLBACK NotificationCallback(
        const PRJ_CALLBACK_DATA* callbackData,
        BOOLEAN isDirectory,
        PRJ_NOTIFICATION notification,
        PCWSTR destinationFileName,
        PRJ_NOTIFICATION_PARAMETERS* notificationParameters
    );

    // Helpers
    std::shared_ptr<VfsTree> tree() const { return tree_; }

private:
    std::shared_ptr<VfsTree> tree_;
    std::wstring target_root_path_;
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT instance_context_{nullptr};
    std::atomic<bool> active_{false};
    GUID virtualization_guid_{};

    static ProjFSMount* instance_from_context(const PRJ_CALLBACK_DATA* callbackData);
};

} // namespace linux2win::vfs
