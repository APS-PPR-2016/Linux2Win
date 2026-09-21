#pragma once

#include "linux2win/types.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <string>

namespace linux2win {

enum class DiskEventType {
    Inserted,
    Removed
};

using DiskEventCallback = std::function<void(DiskEventType type, const DiskInfo& disk)>;

class DiskMonitor {
public:
    DiskMonitor();
    ~DiskMonitor();

    // Start background thread monitoring for disk insertion/removal
    bool start(DiskEventCallback callback);
    void stop();

    bool is_running() const noexcept { return running_; }

private:
    void monitor_thread_proc();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    HWND hwnd_{nullptr};
    HDEVNOTIFY hDevNotify_{nullptr};
    DiskEventCallback callback_;
};

} // namespace linux2win
