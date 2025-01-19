#pragma once

#include "Plugin.hpp"
#include "DynamicLoader.hpp"
#include "FileMonitor.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <deque>
#include <atomic>
#include <map>

namespace core {

class PluginManager {
public:
    static constexpr size_t MAX_BACKUP_FILES = 2;
    static constexpr auto PLUGIN_OPERATION_TIMEOUT = std::chrono::seconds(5);

    PluginManager();
    ~PluginManager();

    // Prevent copying
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // Initialize the plugin manager with a directory to monitor
    void initialize(const std::filesystem::path& pluginDir);

    // Start monitoring for plugin changes
    void start();

    // Stop monitoring
    void stop();

    // Get a plugin by path
    std::shared_ptr<Plugin> getPlugin(const std::string& pluginPath) const;

    // Get all plugins of a specific type
    std::vector<std::shared_ptr<Plugin>> getPluginsByType(PluginType type) const;

private:
    // Callback handlers for file monitoring
    void onNewPlugin(const std::filesystem::path& path);
    void onModifiedPlugin(const std::filesystem::path& path);
    void onDeletedPlugin(const std::filesystem::path& path);
    void onPluginWriteComplete(const std::filesystem::path& path);

    // Backup management
    void manageBackups(const std::filesystem::path& newFile);
    void cleanupOldBackups();
    std::filesystem::path createBackup(const std::filesystem::path& pluginFile);
    void restoreFromBackup();

    // Plugin operations with timeout
    bool loadPluginWithTimeout(const std::filesystem::path& path, bool is_restore = false);
    bool unloadPluginWithTimeout(const std::string& path);

    // Helper functions
    std::vector<std::filesystem::path> getBackupFiles() const;
    bool isPluginFile(const std::filesystem::path& path) const;
    void cleanupPlugins();

    struct PendingDelete {
        std::filesystem::path so_path;
        std::filesystem::path backup_path;
        std::chrono::steady_clock::time_point timestamp;
        bool so_deleted{false};
        bool backup_deleted{false};
    };

    std::mutex pending_deletes_mutex_;
    std::map<std::string, PendingDelete> pending_deletes_;  // base_name -> PendingDelete
    
    void handleBatchedDeletions(const std::string& base_name);
    std::string getBaseName(const std::filesystem::path& path) const;
    void processPendingDelete(const std::string& base_name, const PendingDelete& pending);
    
    static constexpr auto DELETION_BATCH_TIMEOUT = std::chrono::milliseconds(200);

    std::shared_ptr<DynamicLoader> loader_;
    std::shared_ptr<FileMonitor> monitor_;
    std::unordered_map<std::string, std::shared_ptr<Plugin>> plugins_;
    mutable std::mutex plugins_mutex_;
    std::filesystem::path plugin_directory_;
    std::deque<std::filesystem::path> backup_files_;
    mutable std::mutex backup_mutex_;
    std::atomic<bool> is_restoring_{false};  // Flag to track restoration in progress
};

} // namespace core 