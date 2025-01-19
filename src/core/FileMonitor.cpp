#include "FileMonitor.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <iostream>
#include <array>
#include <unordered_map>
#include <unistd.h>
#include <sys/inotify.h>
#include <limits.h>

namespace core {

FileMonitor::FileMonitor() : running(false) {
    // Initialize inotify
    inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd == -1) {
        throw std::runtime_error("Failed to initialize inotify");
    }
}

std::string FileMonitor::calculateFileHash(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read entire file content
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        return "";
    }

    // Simple but effective hash
    uint64_t hash = 14695981039346656037ULL; // FNV-1a hash
    for (char c : buffer) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }

    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

FileMonitor::~FileMonitor() {
    stop();
    close(inotifyFd);
}

void FileMonitor::addWatch(const std::filesystem::path& directory,
                         const std::string& pattern,
                         FileCallback onNewFile,
                         FileCallback onModifiedFile,
                         FileCallback onDeletedFile,
                         FileCallback onCloseWrite) {
    WatchInfo info;
    info.pattern = pattern;
    info.onNewFile = onNewFile;
    info.onModifiedFile = onModifiedFile;
    info.onDeletedFile = onDeletedFile;
    info.onCloseWrite = onCloseWrite;

    // Add inotify watch with IN_MOVED_TO flag to catch file creation via moves
    int wd = inotify_add_watch(inotifyFd, directory.c_str(), 
        IN_CREATE | IN_MODIFY | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd == -1) {
        throw std::runtime_error("Failed to add inotify watch");
    }

    watches[directory] = std::move(info);
    watchDescriptors[wd] = directory;
}

bool FileMonitor::matchesPattern(const std::string& filename, const std::string& pattern) {
    try {
        std::regex regex(pattern);
        return std::regex_match(filename, regex);
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
        return false;
    }
}

void FileMonitor::handleInotifyEvent(const inotify_event* event) {
    auto it = watchDescriptors.find(event->wd);
    if (it == watchDescriptors.end()) return;

    const auto& directory = it->second;
    auto& watch = watches[directory];
    std::filesystem::path filepath = directory / event->name;

    if (!matchesPattern(event->name, watch.pattern)) return;

    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
        if (watch.onNewFile) {
            std::cout << "FileMonitor: New file detected: " << filepath << std::endl;
            watch.onNewFile(filepath);
        }
    }
    if (event->mask & IN_MODIFY) {
        if (watch.onModifiedFile) {
            std::cout << "FileMonitor: File modified: " << filepath << std::endl;
            watch.onModifiedFile(filepath);
        }
    }
    if (event->mask & IN_DELETE) {
        if (watch.onDeletedFile) {
            std::cout << "FileMonitor: File deleted: " << filepath << std::endl;
            watch.onDeletedFile(filepath);
        }
    }
    if (event->mask & IN_CLOSE_WRITE) {
        if (watch.onCloseWrite) {
            std::cout << "FileMonitor: File write completed: " << filepath << std::endl;
            watch.onCloseWrite(filepath);
        }
    }
}

void FileMonitor::monitorLoop() {
    constexpr size_t EVENT_BUF_LEN = 4096;
    std::array<char, EVENT_BUF_LEN> buffer;

    while (running) {
        ssize_t length = read(inotifyFd, buffer.data(), EVENT_BUF_LEN);
        if (length == -1) {
            if (errno == EAGAIN) {
                // No events available, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            std::cerr << "Error reading inotify events" << std::endl;
            break;
        }

        // Process all events in buffer
        char* ptr = buffer.data();
        while (ptr < buffer.data() + length) {
            auto* event = reinterpret_cast<inotify_event*>(ptr);
            handleInotifyEvent(event);
            ptr += sizeof(inotify_event) + event->len;
        }
    }
}

void FileMonitor::start() {
    if (!running.exchange(true)) {
        monitorThread = std::thread(&FileMonitor::monitorLoop, this);
    }
}

void FileMonitor::stop() {
    if (running.exchange(false)) {
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }
}

} // namespace core
