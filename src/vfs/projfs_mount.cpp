#include "linux2win/projfs_mount.hpp"
#include <windows.h>
#include <objbase.h>
#include <projectedfslib.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <filesystem>

#pragma comment(lib, "ole32.lib")

namespace linux2win::vfs {

using pfn_PrjMarkDirectoryAsPlaceholder = HRESULT(STDAPICALLTYPE*)(
    PCWSTR rootPathName,
    PCWSTR targetPathName,
    const PRJ_PLACEHOLDER_VERSION_INFO* versionInfo,
    const GUID* virtualizationInstanceID
);

using pfn_PrjStartVirtualizing = HRESULT(STDAPICALLTYPE*)(
    PCWSTR virtualizationRootPath,
    const PRJ_CALLBACKS* callbacks,
    const void* instanceContext,
    const PRJ_STARTVIRTUALIZING_OPTIONS* options,
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT* namespaceVirtualizationContext
);

using pfn_PrjStopVirtualizing = void(STDAPICALLTYPE*)(
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT namespaceVirtualizationContext
);

using pfn_PrjFillDirEntryBuffer = HRESULT(STDAPICALLTYPE*)(
    PCWSTR fileName,
    PRJ_FILE_BASIC_INFO* fileBasicInfo,
    PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle
);

using pfn_PrjFileNameMatch = BOOLEAN(STDAPICALLTYPE*)(
    PCWSTR fileNamePattern,
    PCWSTR name
);

using pfn_PrjWritePlaceholderInfo = HRESULT(STDAPICALLTYPE*)(
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT namespaceVirtualizationContext,
    PCWSTR destinationFileName,
    const PRJ_PLACEHOLDER_INFO* placeholderInfo,
    UINT32 placeholderInfoSize
);

using pfn_PrjAllocateAlignedBuffer = void*(STDAPICALLTYPE*)(
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT namespaceVirtualizationContext,
    size_t size
);

using pfn_PrjFreeAlignedBuffer = void(STDAPICALLTYPE*)(
    void* buffer
);

using pfn_PrjWriteFileData = HRESULT(STDAPICALLTYPE*)(
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT namespaceVirtualizationContext,
    const GUID* dataStreamId,
    void* buffer,
    UINT64 byteOffset,
    UINT32 length
);

struct ProjFsApi {
    HMODULE hModule{nullptr};
    pfn_PrjMarkDirectoryAsPlaceholder PrjMarkDirectoryAsPlaceholder{nullptr};
    pfn_PrjStartVirtualizing PrjStartVirtualizing{nullptr};
    pfn_PrjStopVirtualizing PrjStopVirtualizing{nullptr};
    pfn_PrjFillDirEntryBuffer PrjFillDirEntryBuffer{nullptr};
    pfn_PrjFileNameMatch PrjFileNameMatch{nullptr};
    pfn_PrjWritePlaceholderInfo PrjWritePlaceholderInfo{nullptr};
    pfn_PrjAllocateAlignedBuffer PrjAllocateAlignedBuffer{nullptr};
    pfn_PrjFreeAlignedBuffer PrjFreeAlignedBuffer{nullptr};
    pfn_PrjWriteFileData PrjWriteFileData{nullptr};

    bool load() {
        if (hModule) return true;
        hModule = LoadLibraryW(L"ProjectedFSLib.dll");
        if (!hModule) return false;

        PrjMarkDirectoryAsPlaceholder = reinterpret_cast<pfn_PrjMarkDirectoryAsPlaceholder>(GetProcAddress(hModule, "PrjMarkDirectoryAsPlaceholder"));
        PrjStartVirtualizing = reinterpret_cast<pfn_PrjStartVirtualizing>(GetProcAddress(hModule, "PrjStartVirtualizing"));
        PrjStopVirtualizing = reinterpret_cast<pfn_PrjStopVirtualizing>(GetProcAddress(hModule, "PrjStopVirtualizing"));
        PrjFillDirEntryBuffer = reinterpret_cast<pfn_PrjFillDirEntryBuffer>(GetProcAddress(hModule, "PrjFillDirEntryBuffer"));
        PrjFileNameMatch = reinterpret_cast<pfn_PrjFileNameMatch>(GetProcAddress(hModule, "PrjFileNameMatch"));
        PrjWritePlaceholderInfo = reinterpret_cast<pfn_PrjWritePlaceholderInfo>(GetProcAddress(hModule, "PrjWritePlaceholderInfo"));
        PrjAllocateAlignedBuffer = reinterpret_cast<pfn_PrjAllocateAlignedBuffer>(GetProcAddress(hModule, "PrjAllocateAlignedBuffer"));
        PrjFreeAlignedBuffer = reinterpret_cast<pfn_PrjFreeAlignedBuffer>(GetProcAddress(hModule, "PrjFreeAlignedBuffer"));
        PrjWriteFileData = reinterpret_cast<pfn_PrjWriteFileData>(GetProcAddress(hModule, "PrjWriteFileData"));

        return (PrjMarkDirectoryAsPlaceholder && PrjStartVirtualizing && PrjStopVirtualizing &&
                PrjFillDirEntryBuffer && PrjWritePlaceholderInfo && PrjWriteFileData);
    }
};

static ProjFsApi g_projfs_api;

struct DirEnumSession {
    std::wstring search_expression;
    std::vector<std::shared_ptr<VfsNode>> matching_entries;
    size_t current_index{0};
};

struct GuidHasher {
    size_t operator()(const GUID& g) const noexcept {
        return std::hash<uint32_t>()(g.Data1) ^ (std::hash<uint16_t>()(g.Data2) << 1) ^ (std::hash<uint16_t>()(g.Data3) << 2);
    }
};

struct GuidEqual {
    bool operator()(const GUID& a, const GUID& b) const noexcept {
        return IsEqualGUID(a, b) != FALSE;
    }
};

static std::unordered_map<GUID, DirEnumSession, GuidHasher, GuidEqual> g_enum_sessions;
static std::mutex g_session_mutex;

static inline LARGE_INTEGER to_large_int(FILETIME ft) {
    LARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return li;
}

ProjFSMount::ProjFSMount(std::shared_ptr<VfsTree> tree, const std::wstring& target_root_path)
    : tree_(tree), target_root_path_(target_root_path) {
    CoCreateGuid(&virtualization_guid_);
}

ProjFSMount::~ProjFSMount() {
    stop();
}

ProjFSMount* ProjFSMount::instance_from_context(const PRJ_CALLBACK_DATA* callbackData) {
    if (!callbackData || !callbackData->InstanceContext) return nullptr;
    return reinterpret_cast<ProjFSMount*>(callbackData->InstanceContext);
}

bool ProjFSMount::start() {
    if (active_) return true;

    if (!g_projfs_api.load()) {
        std::cerr << "\n[!] Windows Projected File System (Client-ProjFS) feature is not enabled.\n";
        std::cerr << "    To enable folder projection in Windows Explorer, run PowerShell as Administrator:\n";
        std::cerr << "    Enable-WindowsOptionalFeature -Online -FeatureName Client-ProjFS\n\n";
        std::cerr << "    Files can still be listed, read, and extracted using 'linux2win ls', 'cat', and 'export'.\n";
        return false;
    }

    // Ensure destination directory exists
    std::filesystem::path p(target_root_path_);
    std::error_code ec;
    std::filesystem::create_directories(p, ec);

    // Mark root directory as a ProjFS placeholder
    HRESULT hr = g_projfs_api.PrjMarkDirectoryAsPlaceholder(
        target_root_path_.c_str(),
        nullptr,
        nullptr,
        &virtualization_guid_
    );

    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        std::wcerr << L"[ProjFS] PrjMarkDirectoryAsPlaceholder failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    PRJ_CALLBACKS callbacks{};
    callbacks.StartDirectoryEnumerationCallback = StartDirEnumCallback;
    callbacks.EndDirectoryEnumerationCallback   = EndDirEnumCallback;
    callbacks.GetDirectoryEnumerationCallback   = GetDirEnumCallback;
    callbacks.GetPlaceholderInfoCallback        = GetPlaceholderInfoCallback;
    callbacks.GetFileDataCallback               = GetFileDataCallback;
    callbacks.CancelCommandCallback             = CancelCommandCallback;
    callbacks.NotificationCallback              = NotificationCallback;

    PRJ_STARTVIRTUALIZING_OPTIONS options{};

    hr = g_projfs_api.PrjStartVirtualizing(
        target_root_path_.c_str(),
        &callbacks,
        this,
        &options,
        &instance_context_
    );

    if (FAILED(hr)) {
        std::wcerr << L"[ProjFS] PrjStartVirtualizing failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    active_ = true;
    std::wcout << L"[ProjFS] Successfully projected Linux filesystem into: " << target_root_path_ << std::endl;
    return true;
}

void ProjFSMount::stop() {
    if (!active_) return;

    active_ = false;
    if (instance_context_ && g_projfs_api.PrjStopVirtualizing) {
        g_projfs_api.PrjStopVirtualizing(instance_context_);
        instance_context_ = nullptr;
    }
}

HRESULT CALLBACK ProjFSMount::StartDirEnumCallback(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID* enumerationId
) {
    auto* self = instance_from_context(callbackData);
    if (!self || !self->tree_) return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

    std::wstring rel_path = callbackData->FilePathName ? callbackData->FilePathName : L"";
    auto node = self->tree_->find_node(rel_path);
    if (!node || !node->is_directory) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    self->tree_->ensure_children_loaded(node);

    DirEnumSession session;
    session.current_index = 0;
    session.matching_entries = node->children;

    std::lock_guard<std::mutex> lock(g_session_mutex);
    g_enum_sessions[*enumerationId] = std::move(session);

    return S_OK;
}

HRESULT CALLBACK ProjFSMount::EndDirEnumCallback(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID* enumerationId
) {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    g_enum_sessions.erase(*enumerationId);
    return S_OK;
}

HRESULT CALLBACK ProjFSMount::GetDirEnumCallback(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID* enumerationId,
    PCWSTR searchExpression,
    PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle
) {
    auto* self = instance_from_context(callbackData);
    if (!self) return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

    std::lock_guard<std::mutex> lock(g_session_mutex);
    auto it = g_enum_sessions.find(*enumerationId);
    if (it == g_enum_sessions.end()) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    auto& session = it->second;

    if (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN) {
        session.current_index = 0;
    }

    if (searchExpression && searchExpression[0] != L'\0') {
        session.search_expression = searchExpression;
    }

    bool entry_added = false;

    while (session.current_index < session.matching_entries.size()) {
        const auto& entry = session.matching_entries[session.current_index];

        std::wstring entry_name_w(entry->name.begin(), entry->name.end());

        // Check if name matches search expression if provided
        if (!session.search_expression.empty() && session.search_expression != L"*") {
            if (g_projfs_api.PrjFileNameMatch) {
                if (!g_projfs_api.PrjFileNameMatch(entry_name_w.c_str(), session.search_expression.c_str())) {
                    session.current_index++;
                    continue;
                }
            }
        }

        PRJ_FILE_BASIC_INFO basicInfo{};
        basicInfo.IsDirectory = entry->is_directory ? TRUE : FALSE;
        basicInfo.FileSize = static_cast<INT64>(entry->file_size);
        basicInfo.CreationTime = to_large_int(entry->creation_time);
        basicInfo.LastAccessTime = to_large_int(entry->last_access_time);
        basicInfo.LastWriteTime = to_large_int(entry->last_write_time);
        basicInfo.ChangeTime = to_large_int(entry->last_write_time);
        // Strictly set read-only attributes
        basicInfo.FileAttributes = entry->file_attributes | FILE_ATTRIBUTE_READONLY;

        HRESULT hr = g_projfs_api.PrjFillDirEntryBuffer(
            entry_name_w.c_str(),
            &basicInfo,
            dirEntryBufferHandle
        );

        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
            if (entry_added) {
                return S_OK;
            }
            return hr;
        }

        if (FAILED(hr)) {
            return hr;
        }

        entry_added = true;
        session.current_index++;
    }

    return entry_added ? S_OK : HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES);
}

HRESULT CALLBACK ProjFSMount::GetPlaceholderInfoCallback(
    const PRJ_CALLBACK_DATA* callbackData
) {
    auto* self = instance_from_context(callbackData);
    if (!self || !self->tree_) return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

    std::wstring rel_path = callbackData->FilePathName ? callbackData->FilePathName : L"";
    auto node = self->tree_->find_node(rel_path);
    if (!node) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    PRJ_PLACEHOLDER_INFO placeholderInfo{};
    placeholderInfo.FileBasicInfo.IsDirectory = node->is_directory ? TRUE : FALSE;
    placeholderInfo.FileBasicInfo.FileSize = static_cast<INT64>(node->file_size);
    placeholderInfo.FileBasicInfo.CreationTime = to_large_int(node->creation_time);
    placeholderInfo.FileBasicInfo.LastAccessTime = to_large_int(node->last_access_time);
    placeholderInfo.FileBasicInfo.LastWriteTime = to_large_int(node->last_write_time);
    placeholderInfo.FileBasicInfo.ChangeTime = to_large_int(node->last_write_time);
    placeholderInfo.FileBasicInfo.FileAttributes = node->file_attributes | FILE_ATTRIBUTE_READONLY;

    return g_projfs_api.PrjWritePlaceholderInfo(
        callbackData->NamespaceVirtualizationContext,
        callbackData->FilePathName,
        &placeholderInfo,
        sizeof(placeholderInfo)
    );
}

HRESULT CALLBACK ProjFSMount::GetFileDataCallback(
    const PRJ_CALLBACK_DATA* callbackData,
    UINT64 byteOffset,
    UINT32 length
) {
    auto* self = instance_from_context(callbackData);
    if (!self || !self->tree_ || !self->tree_->reader()) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    std::wstring rel_path = callbackData->FilePathName ? callbackData->FilePathName : L"";
    auto node = self->tree_->find_node(rel_path);
    if (!node || node->is_directory) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    void* buffer = nullptr;
    if (g_projfs_api.PrjAllocateAlignedBuffer) {
        buffer = g_projfs_api.PrjAllocateAlignedBuffer(callbackData->NamespaceVirtualizationContext, length);
    }
    if (!buffer) {
        return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
    }

    size_t bytes_read = 0;
    bool ok = self->tree_->reader()->read_file_data(
        node->inode_num,
        byteOffset,
        buffer,
        length,
        &bytes_read
    );

    if (!ok) {
        if (g_projfs_api.PrjFreeAlignedBuffer) g_projfs_api.PrjFreeAlignedBuffer(buffer);
        return HRESULT_FROM_WIN32(ERROR_READ_FAULT);
    }

    HRESULT hr = g_projfs_api.PrjWriteFileData(
        callbackData->NamespaceVirtualizationContext,
        &callbackData->DataStreamId,
        buffer,
        byteOffset,
        static_cast<UINT32>(bytes_read)
    );

    if (g_projfs_api.PrjFreeAlignedBuffer) g_projfs_api.PrjFreeAlignedBuffer(buffer);
    return hr;
}

void CALLBACK ProjFSMount::CancelCommandCallback(
    const PRJ_CALLBACK_DATA* callbackData
) {
}

HRESULT CALLBACK ProjFSMount::NotificationCallback(
    const PRJ_CALLBACK_DATA* callbackData,
    BOOLEAN isDirectory,
    PRJ_NOTIFICATION notification,
    PCWSTR destinationFileName,
    PRJ_NOTIFICATION_PARAMETERS* notificationParameters
) {
    // Protect against writes / deletes: Any attempt to modify returns Access Denied
    if (notification == PRJ_NOTIFICATION_FILE_OPENED ||
        notification == PRJ_NOTIFICATION_NEW_FILE_CREATED ||
        notification == PRJ_NOTIFICATION_FILE_OVERWRITTEN ||
        notification == PRJ_NOTIFICATION_PRE_DELETE ||
        notification == PRJ_NOTIFICATION_PRE_RENAME ||
        notification == PRJ_NOTIFICATION_PRE_SET_HARDLINK) {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    return S_OK;
}

} // namespace linux2win::vfs
