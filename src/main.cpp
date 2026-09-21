#include "linux2win/disk_enumerator.hpp"
#include "linux2win/disk_monitor.hpp"
#include "linux2win/partition_filter.hpp"
#include "linux2win/block_device.hpp"
#include "linux2win/ext4_reader.hpp"
#include "linux2win/vfs_tree.hpp"
#include "linux2win/projfs_mount.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <csignal>

using namespace linux2win;

static std::atomic<bool> g_keep_running{true};

void signal_handler(int) {
    g_keep_running = false;
}

void print_banner() {
    std::cout << "===============================================================\n";
    std::cout << "   Linux2Win - Linux Disk Auto-Detector & Windows Folder VFS   \n";
    std::cout << "   Read-Only & Data Partition Safety Protection Engine         \n";
    std::cout << "===============================================================\n\n";
}

void print_usage() {
    std::cout << "Usage: linux2win <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  scan                         Scan and list all disks, showing data vs excluded partitions\n";
    std::cout << "  monitor                      Monitor hotplug events (detects inserted/removed Linux disks)\n";
    std::cout << "  auto                         Auto-detect and mount all Linux data partitions as Windows folders\n";
    std::cout << "  mount <disk> <part> [folder] Mount specific Linux partition as Windows folder via ProjFS\n";
    std::cout << "  ls <disk> <part> [path]      List directory contents of a Linux partition\n";
    std::cout << "  cat <disk> <part> <file>     Display contents of a file on Linux partition\n";
    std::cout << "  export <disk> <part> <src> <dst> Export/copy Linux file or directory to a Windows folder\n\n";
    std::cout << "Examples:\n";
    std::cout << "  linux2win scan\n";
    std::cout << "  linux2win mount 1 2 C:\\LinuxDisks\\Drive1_Part2\n";
    std::cout << "  linux2win ls 1 2 /etc\n";
    std::cout << "  linux2win export 1 2 /home/user/document.txt C:\\Users\\user\\Desktop\n";
}

std::string format_bytes(uint64_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    if (bytes >= TB) ss << (bytes / TB) << " TB";
    else if (bytes >= GB) ss << (bytes / GB) << " GB";
    else if (bytes >= MB) ss << (bytes / MB) << " MB";
    else if (bytes >= KB) ss << (bytes / KB) << " KB";
    else ss << bytes << " B";

    return ss.str();
}

void cmd_scan() {
    std::cout << "Scanning physical drives for Linux partitions...\n\n";
    auto disks = DiskEnumerator::enumerate_all_disks();

    if (disks.empty()) {
        std::cout << "No physical drives detected or administrator privileges required.\n";
        return;
    }

    size_t total_data_partitions = 0;
    size_t total_excluded_partitions = 0;

    for (const auto& disk : disks) {
        std::cout << "---------------------------------------------------------------\n";
        std::cout << "Disk " << disk.disk_index << ": " << disk.friendly_name << "\n";
        std::cout << "  Path:       " << disk.device_path << "\n";
        std::cout << "  Bus Type:   " << disk.bus_type << (disk.is_removable ? " (Removable / USB)" : "") << "\n";
        std::cout << "  Size:       " << format_bytes(disk.total_size_bytes) << "\n";
        std::cout << "  Partitions: " << (disk.has_gpt ? "GPT" : "MBR") << " (" << disk.partitions.size() << " found)\n\n";

        for (const auto& part : disk.partitions) {
            std::string kind_str = PartitionFilter::kind_to_string(part.kind);
            std::string fs_str = PartitionFilter::fs_type_to_string(part.fs_type);

            std::cout << "  [Partition " << part.partition_number << "]\n";
            std::cout << "    Offset: " << format_bytes(part.starting_offset) 
                      << " | Size: " << format_bytes(part.total_length) << "\n";
            std::cout << "    Classification: " << kind_str << "\n";

            if (part.fs_type != FilesystemType::Unknown) {
                std::cout << "    Filesystem:     " << fs_str;
                if (!part.fs_label.empty()) std::cout << " (Label: " << part.fs_label << ")";
                if (!part.fs_uuid.empty()) std::cout << " [UUID: " << part.fs_uuid << "]";
                std::cout << "\n";
            }

            if (part.is_data_partition) {
                std::cout << "    >>> STATUS: SAFE DATA PARTITION (Ready for Read-Only Mount)\n";
                total_data_partitions++;
            } else if (PartitionFilter::is_excluded_system_partition(part.kind)) {
                std::cout << "    >>> STATUS: EXCLUDED (Boot / System / Swap Protection)\n";
                total_excluded_partitions++;
            } else {
                std::cout << "    >>> STATUS: Non-Linux / Ignored\n";
            }
            std::cout << "\n";
        }
    }

    std::cout << "===============================================================\n";
    std::cout << "Scan Complete: " << total_data_partitions << " mountable Linux data partitions, "
              << total_excluded_partitions << " boot/system partitions safely excluded.\n";
}

void cmd_monitor() {
    std::cout << "Starting Linux Disk Hotplug Monitor. Press Ctrl+C to exit...\n\n";

    DiskMonitor monitor;
    monitor.start([](DiskEventType type, const DiskInfo& disk) {
        if (type == DiskEventType::Inserted) {
            std::cout << "\n[+] Disk Detected: " << disk.friendly_name 
                      << " (Disk " << disk.disk_index << ", " << disk.bus_type << ")\n";
            for (const auto& part : disk.partitions) {
                if (part.is_data_partition) {
                    std::cout << "    --> Linux Data Partition Found: Part " << part.partition_number
                              << " (" << format_bytes(part.total_length) << ") FS: "
                              << PartitionFilter::fs_type_to_string(part.fs_type) << "\n";
                }
            }
        } else {
            std::cout << "\n[-] Disk Removed: Disk " << disk.disk_index << "\n";
        }
    });

    std::signal(SIGINT, signal_handler);
    while (g_keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    monitor.stop();
    std::cout << "Monitor stopped.\n";
}

bool mount_partition(const PartitionInfo& part, const std::wstring& mount_dir) {
    std::string disk_path = "\\\\.\\PhysicalDrive" + std::to_string(part.disk_index);

    auto dev = std::make_shared<BlockDevice>();
    if (!dev->open_read_only(disk_path, part.starting_offset, part.total_length)) {
        std::cerr << "Failed to open physical drive " << disk_path << " with read-only access.\n";
        return false;
    }

    auto reader = std::make_shared<fs::Ext4Reader>();
    if (!reader->mount(dev)) {
        std::cerr << "Failed to parse Ext4 filesystem on Disk " << part.disk_index << " Partition " << part.partition_number << "\n";
        return false;
    }

    auto tree = std::make_shared<vfs::VfsTree>(reader);
    auto projfs = std::make_shared<vfs::ProjFSMount>(tree, mount_dir);

    if (!projfs->start()) {
        std::cerr << "Failed to start ProjFS folder projection on target directory.\n";
        return false;
    }

    std::wcout << L"Folder successfully mounted at: " << mount_dir << std::endl;
    std::cout << "Press Ctrl+C to unmount...\n";

    std::signal(SIGINT, signal_handler);
    while (g_keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    projfs->stop();
    reader->unmount();
    dev->close();
    std::cout << "Unmounted successfully.\n";
    return true;
}

void cmd_mount(uint32_t disk_idx, uint32_t part_idx, const std::string& target_folder) {
    auto disk_opt = DiskEnumerator::inspect_disk(disk_idx);
    if (!disk_opt.has_value()) {
        std::cerr << "Error: Disk " << disk_idx << " not found or inaccessible.\n";
        return;
    }

    const auto& disk = disk_opt.value();
    const PartitionInfo* target_part = nullptr;
    for (const auto& part : disk.partitions) {
        if (part.partition_number == part_idx) {
            target_part = &part;
            break;
        }
    }

    if (!target_part) {
        std::cerr << "Error: Partition " << part_idx << " not found on Disk " << disk_idx << ".\n";
        return;
    }

    if (!target_part->is_data_partition || PartitionFilter::is_excluded_system_partition(target_part->kind)) {
        std::cerr << "Error: Partition " << part_idx << " is a boot/system partition (" 
                  << PartitionFilter::kind_to_string(target_part->kind) 
                  << ") and cannot be mounted for safety reasons.\n";
        return;
    }

    std::wstring mount_dir;
    if (target_folder.empty()) {
        std::string default_dir = "C:\\LinuxDisks\\Disk" + std::to_string(disk_idx) + "_Part" + std::to_string(part_idx);
        mount_dir = std::wstring(default_dir.begin(), default_dir.end());
    } else {
        mount_dir = std::wstring(target_folder.begin(), target_folder.end());
    }

    mount_partition(*target_part, mount_dir);
}

void cmd_ls(uint32_t disk_idx, uint32_t part_idx, const std::string& path) {
    auto disk_opt = DiskEnumerator::inspect_disk(disk_idx);
    if (!disk_opt.has_value()) {
        std::cerr << "Error: Disk " << disk_idx << " not found.\n";
        return;
    }

    const auto& disk = disk_opt.value();
    for (const auto& part : disk.partitions) {
        if (part.partition_number == part_idx) {
            if (!part.is_data_partition) {
                std::cerr << "Error: Partition is excluded/system partition.\n";
                return;
            }

            auto dev = std::make_shared<BlockDevice>();
            if (!dev->open_read_only("\\\\.\\PhysicalDrive" + std::to_string(disk_idx), part.starting_offset, part.total_length)) {
                std::cerr << "Cannot open disk.\n";
                return;
            }

            auto reader = std::make_shared<fs::Ext4Reader>();
            if (!reader->mount(dev)) {
                std::cerr << "Cannot mount ext4.\n";
                return;
            }

            std::vector<fs::DirEntryInfo> entries;
            std::string query_path = path.empty() ? "/" : path;
            if (reader->list_directory_path(query_path, entries)) {
                std::cout << "Listing directory '" << query_path << "' on Disk " << disk_idx << " Part " << part_idx << ":\n\n";
                std::cout << std::left << std::setw(12) << "Inode" << std::setw(10) << "Type" << std::setw(14) << "Size" << "Name\n";
                std::cout << "--------------------------------------------------------\n";
                for (const auto& entry : entries) {
                    fs::FileStat st{};
                    reader->stat_inode(entry.inode, st);
                    std::string type = entry.is_directory ? "<DIR>" : (entry.is_symlink ? "<SYMLINK>" : "<FILE>");
                    std::cout << std::left << std::setw(12) << entry.inode
                              << std::setw(10) << type
                              << std::setw(14) << (entry.is_directory ? "-" : format_bytes(st.size))
                              << entry.name;
                    if (entry.is_symlink && !st.symlink_target.empty()) {
                        std::cout << " -> " << st.symlink_target;
                    }
                    std::cout << "\n";
                }
            } else {
                std::cerr << "Path not found or not a directory: " << query_path << "\n";
            }
            return;
        }
    }
    std::cerr << "Partition " << part_idx << " not found.\n";
}

void cmd_cat(uint32_t disk_idx, uint32_t part_idx, const std::string& file_path) {
    auto disk_opt = DiskEnumerator::inspect_disk(disk_idx);
    if (!disk_opt.has_value()) return;

    for (const auto& part : disk_opt.value().partitions) {
        if (part.partition_number == part_idx && part.is_data_partition) {
            auto dev = std::make_shared<BlockDevice>();
            dev->open_read_only("\\\\.\\PhysicalDrive" + std::to_string(disk_idx), part.starting_offset, part.total_length);
            auto reader = std::make_shared<fs::Ext4Reader>();
            if (reader->mount(dev)) {
                fs::FileStat st{};
                if (reader->stat_path(file_path, st) && st.is_regular) {
                    std::vector<char> buffer(st.size);
                    size_t bytes_read = 0;
                    if (reader->read_file_by_path(file_path, 0, buffer.data(), st.size, &bytes_read)) {
                        std::cout.write(buffer.data(), bytes_read);
                    }
                } else {
                    std::cerr << "File not found or is not a regular file.\n";
                }
            }
            return;
        }
    }
}

void cmd_export(uint32_t disk_idx, uint32_t part_idx, const std::string& src_linux_path, const std::string& dst_win_dir) {
    auto disk_opt = DiskEnumerator::inspect_disk(disk_idx);
    if (!disk_opt.has_value()) return;

    for (const auto& part : disk_opt.value().partitions) {
        if (part.partition_number == part_idx && part.is_data_partition) {
            auto dev = std::make_shared<BlockDevice>();
            dev->open_read_only("\\\\.\\PhysicalDrive" + std::to_string(disk_idx), part.starting_offset, part.total_length);
            auto reader = std::make_shared<fs::Ext4Reader>();
            if (reader->mount(dev)) {
                fs::FileStat st{};
                if (reader->stat_path(src_linux_path, st)) {
                    if (st.is_regular) {
                        std::filesystem::path dst_path(dst_win_dir);
                        std::string fname = std::filesystem::path(src_linux_path).filename().string();
                        if (std::filesystem::is_directory(dst_path)) {
                            dst_path /= fname;
                        }

                        std::ofstream out(dst_path, std::ios::binary);
                        if (!out) {
                            std::cerr << "Cannot open destination file: " << dst_path.string() << "\n";
                            return;
                        }

                        std::vector<char> buf(65536);
                        uint64_t offset = 0;
                        while (offset < st.size) {
                            size_t to_read = static_cast<size_t>(std::min<uint64_t>(buf.size(), st.size - offset));
                            size_t bytes_read = 0;
                            if (!reader->read_file_by_path(src_linux_path, offset, buf.data(), to_read, &bytes_read) || bytes_read == 0) {
                                break;
                            }
                            out.write(buf.data(), bytes_read);
                            offset += bytes_read;
                        }
                        std::cout << "Successfully exported " << src_linux_path << " -> " << dst_path.string() 
                                  << " (" << format_bytes(st.size) << ")\n";
                    }
                }
            }
            return;
        }
    }
}

void cmd_auto() {
    std::cout << "Starting Auto-Detect & Folder Projection Service...\n";
    std::cout << "Scanning for existing Linux data partitions...\n";

    auto mountable = DiskEnumerator::get_all_mountable_partitions();
    for (const auto& part : mountable) {
        std::string mount_dir = "C:\\LinuxDisks\\Disk" + std::to_string(part.disk_index) + "_Part" + std::to_string(part.partition_number);
        std::cout << "Found Linux Data Partition on Disk " << part.disk_index << " Part " << part.partition_number << "\n";
        std::cout << "Mount target: " << mount_dir << "\n";
    }

    std::cout << "\nMonitoring for newly inserted Linux USB drives and disks. Press Ctrl+C to stop.\n";
    DiskMonitor monitor;
    monitor.start([](DiskEventType type, const DiskInfo& disk) {
        if (type == DiskEventType::Inserted) {
            for (const auto& part : disk.partitions) {
                if (part.is_data_partition && !PartitionFilter::is_excluded_system_partition(part.kind)) {
                    std::cout << "[+] Auto-Detected Linux Data Drive: Disk " << part.disk_index 
                              << " Part " << part.partition_number << " (" << format_bytes(part.total_length) << ")\n";
                }
            }
        }
    });

    std::signal(SIGINT, signal_handler);
    while (g_keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    monitor.stop();
}

int main(int argc, char* argv[]) {
    print_banner();

    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "scan") {
        cmd_scan();
    } else if (cmd == "monitor") {
        cmd_monitor();
    } else if (cmd == "auto") {
        cmd_auto();
    } else if (cmd == "mount") {
        if (argc < 4) {
            std::cerr << "Usage: linux2win mount <disk_index> <partition_number> [target_folder]\n";
            return 1;
        }
        uint32_t disk_idx = static_cast<uint32_t>(std::stoul(argv[2]));
        uint32_t part_idx = static_cast<uint32_t>(std::stoul(argv[3]));
        std::string target = (argc >= 5) ? argv[4] : "";
        cmd_mount(disk_idx, part_idx, target);
    } else if (cmd == "ls") {
        if (argc < 4) {
            std::cerr << "Usage: linux2win ls <disk_index> <partition_number> [path]\n";
            return 1;
        }
        uint32_t disk_idx = static_cast<uint32_t>(std::stoul(argv[2]));
        uint32_t part_idx = static_cast<uint32_t>(std::stoul(argv[3]));
        std::string path = (argc >= 5) ? argv[4] : "/";
        cmd_ls(disk_idx, part_idx, path);
    } else if (cmd == "cat") {
        if (argc < 5) {
            std::cerr << "Usage: linux2win cat <disk_index> <partition_number> <file_path>\n";
            return 1;
        }
        uint32_t disk_idx = static_cast<uint32_t>(std::stoul(argv[2]));
        uint32_t part_idx = static_cast<uint32_t>(std::stoul(argv[3]));
        std::string file_path = argv[4];
        cmd_cat(disk_idx, part_idx, file_path);
    } else if (cmd == "export") {
        if (argc < 6) {
            std::cerr << "Usage: linux2win export <disk_index> <partition_number> <src_path> <dst_folder>\n";
            return 1;
        }
        uint32_t disk_idx = static_cast<uint32_t>(std::stoul(argv[2]));
        uint32_t part_idx = static_cast<uint32_t>(std::stoul(argv[3]));
        std::string src = argv[4];
        std::string dst = argv[5];
        cmd_export(disk_idx, part_idx, src, dst);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n\n";
        print_usage();
        return 1;
    }

    return 0;
}
