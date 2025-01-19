#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <sys/inotify.h>
#include <unordered_map>

namespace core {

class FileMonitor {
public:
    using FileCallback = std::function<void(const std::filesystem::path&)>;

    FileMonitor();
    ~FileMonitor();

    // Prevent copying
    FileMonitor(const FileMonitor&) = delete;
    FileMonitor& operator=(const FileMonitor&) = delete;

    // Add a directory to monitor with file pattern and callback
    void addWatch(const std::filesystem::path& directory, 
                 const std::string& pattern,
                 FileCallback onNewFile,
                 FileCallback onModifiedFile,
                 FileCallback onDeletedFile,
                 FileCallback onCloseWrite);

    // Start monitoring
    void start();

    // Stop monitoring
    void stop();

private:
    struct FileInfo {
        std::string contentHash;  // Hash of file content
        std::filesystem::file_time_type lastModified;
        int watchDescriptor;  // inotify watch descriptor
    };

    struct WatchInfo {
        std::string pattern;
        FileCallback onNewFile;
        FileCallback onModifiedFile;
        FileCallback onDeletedFile;
        FileCallback onCloseWrite;
        std::unordered_map<std::filesystem::path, FileInfo> files;
    };

    bool matchesPattern(const std::string& filename, const std::string& pattern);
    void monitorLoop();
    std::string calculateFileHash(const std::filesystem::path& path);
    void handleInotifyEvent(const inotify_event* event);

    std::unordered_map<std::filesystem::path, WatchInfo> watches;
    std::unordered_map<int, std::filesystem::path> watchDescriptors;  // maps watch descriptors to paths
    std::atomic<bool> running;
    std::thread monitorThread;
    int inotifyFd;  // inotify file descriptor
};

} // namespace core
