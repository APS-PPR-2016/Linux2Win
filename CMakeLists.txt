cmake_minimum_required(VERSION 3.20)

project(Linux2Win
    VERSION 1.0.0
    DESCRIPTION "Linux Disk Auto-Detector & Windows Folder Projection VFS"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
include(CTest)
enable_testing()

# Compiler options
if(MSVC)
    add_compile_options(
        /W3
        /EHsc
        /utf-8
        /permissive-
        /Zc:__cplusplus
    )
    add_compile_definitions(
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Core library sources
set(CORE_SOURCES
    src/disk/partition_filter.cpp
    src/disk/disk_enumerator.cpp
    src/disk/disk_monitor.cpp
    src/fs/block_device.cpp
    src/fs/ext4_reader.cpp
    src/vfs/vfs_tree.cpp
    src/vfs/projfs_mount.cpp
    src/service/startup_manager.cpp
    src/service/tray_icon.cpp
)

set(CORE_HEADERS
    include/linux2win/types.hpp
    include/linux2win/partition_filter.hpp
    include/linux2win/disk_enumerator.hpp
    include/linux2win/disk_monitor.hpp
    include/linux2win/block_device.hpp
    include/linux2win/ext4_types.hpp
    include/linux2win/ext4_reader.hpp
    include/linux2win/vfs_tree.hpp
    include/linux2win/projfs_mount.hpp
    include/linux2win/startup_manager.hpp
    include/linux2win/tray_icon.hpp
)

add_library(linux2win_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(linux2win_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(linux2win_core PUBLIC
    setupapi
    user32
    ole32
    shell32
    comctl32
    advapi32
    shlwapi
)

# CLI Application
add_executable(linux2win
    src/main.cpp
)
target_link_libraries(linux2win PRIVATE
    linux2win_core
)

# Memory-Resident Background Daemon (System Tray)
add_executable(linux2win_daemon WIN32
    src/service/daemon_main.cpp
)
target_link_libraries(linux2win_daemon PRIVATE
    linux2win_core
)

# Windows Setup Wizard & Installer
add_executable(linux2win_setup WIN32
    src/installer/setup_main.cpp
)
target_link_libraries(linux2win_setup PRIVATE
    linux2win_core
)

# Unit Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS linux2win linux2win_daemon linux2win_setup linux2win_core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# CPack Packaging Configuration
set(CPACK_PACKAGE_NAME "Linux2Win")
set(CPACK_PACKAGE_VENDOR "Linux2Win Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux Disk Auto-Detector & Windows Folder VFS")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Linux2Win")
set(CPACK_GENERATOR "ZIP;NSIS")

include(CPack)
