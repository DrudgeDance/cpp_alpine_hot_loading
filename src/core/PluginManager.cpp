#include "PluginManager.hpp"
#include "../plugins/endpoints/EndpointPlugin.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <algorithm>
#include <map>
#include <fstream>

using namespace plugins::endpoint;

namespace core {

PluginManager::PluginManager()
    : loader_(std::make_shared<DynamicLoader>())
    , monitor_(std::make_shared<FileMonitor>()) {
}

PluginManager::~PluginManager() {
    stop();
    cleanupPlugins();
}

void PluginManager::cleanupPlugins() {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    plugins_.clear(); // This will trigger plugin cleanup through shared_ptr
}

void PluginManager::initialize(const std::filesystem::path& pluginDir) {
    plugin_directory_ = std::filesystem::absolute(pluginDir);
    
    if (!std::filesystem::exists(plugin_directory_)) {
        std::filesystem::create_directories(plugin_directory_);
    }

    // Set up file monitoring for .so files
    monitor_->addWatch(
        plugin_directory_,
        ".*\\.so$",
        [this](const std::filesystem::path& path) { 
            std::cout << "New file detected: " << path << std::endl;
            onPluginWriteComplete(path); 
        },
        [this](const std::filesystem::path& path) { 
            std::cout << "File modified: " << path << std::endl;
            onPluginWriteComplete(path); 
        },
        [this](const std::filesystem::path& path) { onDeletedPlugin(path); },
        [this](const std::filesystem::path& path) { onPluginWriteComplete(path); }
    );

    // Load any existing plugins
    cleanupOldBackups(); // Clean up old backups before loading
    
    // Find the newest .so file
    std::filesystem::path newest_plugin;
    std::filesystem::file_time_type newest_time;
    
    for (const auto& entry : std::filesystem::directory_iterator(plugin_directory_)) {
        if (isPluginFile(entry.path())) {
            auto mod_time = std::filesystem::last_write_time(entry.path());
            if (newest_plugin.empty() || mod_time > newest_time) {
                newest_plugin = entry.path();
                newest_time = mod_time;
            }
        }
    }
    
    // Load the newest plugin if found
    if (!newest_plugin.empty()) {
        std::cout << "Loading newest plugin: " << newest_plugin << std::endl;
        onPluginWriteComplete(newest_plugin);
    }
}

bool PluginManager::isPluginFile(const std::filesystem::path& path) const {
    return path.extension() == ".so";
}

void PluginManager::start() {
    monitor_->start();
}

void PluginManager::stop() {
    monitor_->stop();
}

std::shared_ptr<Plugin> PluginManager::getPlugin(const std::string& pluginPath) const {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    auto abs_path = std::filesystem::absolute(pluginPath).string();
    auto it = plugins_.find(abs_path);
    return (it != plugins_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Plugin>> PluginManager::getPluginsByType(PluginType type) const {
    std::vector<std::shared_ptr<Plugin>> result;
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    std::cout << "Looking for plugins of type " << static_cast<int>(type) 
              << ", total plugins loaded: " << plugins_.size() << std::endl;
    
    for (const auto& [path, plugin] : plugins_) {
        std::cout << "Checking plugin at path: " << path << std::endl;
        if (plugin->getType() == type) {
            std::cout << "Found matching plugin of requested type" << std::endl;
            result.push_back(plugin);
        }
    }
    
    return result;
}

bool PluginManager::loadPluginWithTimeout(const std::filesystem::path& path, bool is_restore) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Plugin file does not exist: " << path << std::endl;
        return false;
    }

    // Check if file is valid before attempting to load
    try {
        auto file_size = std::filesystem::file_size(path);
        if (file_size < 64) {  // Minimum size for a valid .so file
            std::cerr << "Plugin file is too small to be valid: " << path << std::endl;
            return false;
        }
        
        // For restored plugins, verify file integrity
        if (is_restore) {
            std::ifstream file(path, std::ios::binary);
            if (!file.good()) {
                std::cerr << "Cannot open restored plugin file: " << path << std::endl;
                return false;
            }
            
            // Read first few bytes to verify it's a valid ELF file
            char magic[4];
            file.read(magic, 4);
            if (!file || magic[0] != 0x7f || magic[1] != 'E' || magic[2] != 'L' || magic[3] != 'F') {
                std::cerr << "Restored file is not a valid ELF file: " << path << std::endl;
                return false;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error checking plugin file: " << e.what() << std::endl;
        return false;
    }

    // Use a shorter timeout for restored plugins since we've already verified the file
    const auto timeout = is_restore ? std::chrono::seconds(5) : std::chrono::seconds(5);
    
    // Use a promise to signal completion
    std::promise<bool> loading_promise;
    auto loading_future = loading_promise.get_future();
    
    // Create a flag to signal the worker thread to stop
    std::atomic<bool> should_stop{false};
    
    // Launch the worker thread
    std::thread worker([this, path, &loading_promise, &should_stop]() {
        try {
            // First try to load the plugin
            std::cout << "Attempting to load plugin: " << path << std::endl;
            
            // Add a small delay before loading to ensure file system operations are complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (should_stop) {
                loading_promise.set_value(false);
                return;
            }
            
            auto plugin = loader_->loadPlugin(path);
            if (!plugin) {
                std::cerr << "Failed to load plugin (null pointer returned)" << std::endl;
                loading_promise.set_value(false);
                return;
            }

            if (should_stop) {
                loading_promise.set_value(false);
                return;
            }

            try {
                // Initialize the plugin before storing it
                std::cout << "Initializing plugin..." << std::endl;
                plugin->initialize();
                
                if (should_stop) {
                    loading_promise.set_value(false);
                    return;
                }
                
                {
                    std::lock_guard<std::mutex> lock(plugins_mutex_);
                    plugins_[path.string()] = plugin;
                }
                std::cout << "Successfully loaded and initialized plugin: " << path << std::endl;
                loading_promise.set_value(true);
            } catch (const std::exception& e) {
                std::cerr << "Error initializing plugin: " << e.what() << std::endl;
                loading_promise.set_value(false);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading plugin: " << e.what() << std::endl;
            try {
                loading_promise.set_value(false);
            } catch (...) {}
        }
    });

    // Wait for completion or timeout
    bool success = false;
    try {
        auto status = loading_future.wait_for(timeout);
        if (status == std::future_status::timeout) {
            std::cerr << "Plugin loading timed out after " << timeout.count() 
                     << " seconds: " << path << std::endl;
            // Signal the worker thread to stop
            should_stop = true;
        } else {
            success = loading_future.get();
            if (!success) {
                std::cerr << "Plugin loading failed: " << path << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during plugin loading: " << e.what() << std::endl;
        should_stop = true;
    }

    // Clean up the worker thread
    if (worker.joinable()) {
        if (should_stop) {
            // Give the thread a moment to clean up
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        worker.join();
    }

    // If loading failed or timed out, try to clean up
    if (!success) {
        try {
            std::lock_guard<std::mutex> lock(plugins_mutex_);
            plugins_.erase(path.string());
            loader_->unloadPlugin(path.string());
        } catch (const std::exception& e) {
            std::cerr << "Error cleaning up failed plugin load: " << e.what() << std::endl;
        }
    }

    return success;
}

bool PluginManager::unloadPluginWithTimeout(const std::string& path) {
    auto future = std::async(std::launch::async, [this, path]() {
        try {
            std::lock_guard<std::mutex> lock(plugins_mutex_);
            plugins_.erase(path);
            loader_->unloadPlugin(path);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error unloading plugin: " << e.what() << std::endl;
            return false;
        }
    });

    if (future.wait_for(PLUGIN_OPERATION_TIMEOUT) == std::future_status::timeout) {
        std::cerr << "Plugin unloading timed out: " << path << std::endl;
        return false;
    }

    return future.get();
}

void PluginManager::manageBackups(const std::filesystem::path& newFile) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    
    // Create backup of the new file
    auto backup = createBackup(newFile);
    if (!backup.empty()) {
        // Get the base name without timestamp
        std::string base_name = newFile.stem().string();
        base_name = base_name.substr(0, base_name.find_last_of('_'));
        
        // Remove any existing backups of this plugin type
        backup_files_.erase(
            std::remove_if(backup_files_.begin(), backup_files_.end(),
                [&base_name](const auto& path) {
                    std::string existing_base = path.stem().string();
                    existing_base = existing_base.substr(0, existing_base.find_last_of('_'));
                    return existing_base == base_name;
                }),
            backup_files_.end()
        );
        
        // Add new backup
        backup_files_.push_front(backup);
        std::cout << "Created backup: " << backup << std::endl;
        
        // Keep only the most recent MAX_BACKUP_FILES backups across all plugin types
        while (backup_files_.size() > MAX_BACKUP_FILES) {
            auto oldest = backup_files_.back();
            if (std::filesystem::exists(oldest)) {
                std::filesystem::remove(oldest);
                std::cout << "Removed old backup: " << oldest << std::endl;
            }
            backup_files_.pop_back();
        }
    }
}

std::filesystem::path PluginManager::createBackup(const std::filesystem::path& pluginFile) {
    try {
        auto backup_path = pluginFile;
        backup_path += ".backup";
        std::filesystem::copy_file(pluginFile, backup_path, 
                                 std::filesystem::copy_options::overwrite_existing);
        return backup_path;
    } catch (const std::exception& e) {
        std::cerr << "Error creating backup: " << e.what() << std::endl;
        return {};
    }
}

void PluginManager::cleanupOldBackups() {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    
    // Find all backup files
    std::vector<std::filesystem::path> backups;
    for (const auto& entry : std::filesystem::directory_iterator(plugin_directory_)) {
        if (entry.path().extension() == ".backup") {
            backups.push_back(entry.path());
        }
    }

    // Group backups by plugin type
    std::map<std::string, std::vector<std::filesystem::path>> grouped_backups;
    for (const auto& backup : backups) {
        std::string base_name = backup.stem().string();
        base_name = base_name.substr(0, base_name.find_last_of('_'));
        grouped_backups[base_name].push_back(backup);
    }

    // Keep only the most recent backups for each plugin type
    backup_files_.clear();
    for (auto& [base_name, type_backups] : grouped_backups) {
        // Sort by modification time (newest first)
        std::sort(type_backups.begin(), type_backups.end(),
                 [](const auto& a, const auto& b) {
                     return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
                 });
        
        // Keep the newest backup for this plugin type
        if (!type_backups.empty()) {
            backup_files_.push_back(type_backups.front());
            
            // Remove any excess backups for this plugin type
            for (size_t i = 1; i < type_backups.size(); ++i) {
                if (std::filesystem::exists(type_backups[i])) {
                    std::filesystem::remove(type_backups[i]);
                    std::cout << "Removed old backup during cleanup: " << type_backups[i] << std::endl;
                }
            }
        }
    }
}

void PluginManager::restoreFromBackup() {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    
    // Find all .so and .so.backup files in the directory
    std::vector<std::filesystem::path> candidates;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(plugin_directory_)) {
            if (entry.path().extension() == ".so" || 
                (entry.path().extension() == ".backup" && entry.path().stem().extension() == ".so")) {
                candidates.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error scanning directory for candidates: " << e.what() << std::endl;
        return;
    }

    if (candidates.empty()) {
        std::cerr << "No .so or .so.backup files found in directory" << std::endl;
        return;
    }

    // Sort by modification time (newest first)
    std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) {
                try {
                    return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
                } catch (const std::filesystem::filesystem_error&) {
                    return false;
                }
            });

    // Set the restoration flag
    is_restoring_ = true;

    // Try each file in order until one works
    for (const auto& candidate : candidates) {
        std::cout << "Attempting to restore using: " << candidate << std::endl;
        
        try {
            std::filesystem::path target_path;
            if (candidate.extension() == ".backup") {
                // If it's a backup file, we need to restore it to a .so file
                target_path = candidate;
                target_path.replace_extension("");  // Remove .backup extension
            } else {
                // If it's already a .so file, just try to load it directly
                target_path = candidate;
            }

            if (candidate.extension() == ".backup") {
                // Only need to copy if it's a backup file
                try {
                    std::filesystem::copy_file(candidate, target_path, 
                                             std::filesystem::copy_options::overwrite_existing);
                    std::cout << "Copied backup file to: " << target_path << std::endl;
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error copying backup file: " << e.what() << std::endl;
                    continue;
                }
            }

            // Give the file system a moment to settle
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Try to load the plugin
            if (loadPluginWithTimeout(target_path, true)) {
                std::cout << "Successfully restored and loaded plugin from: " << candidate << std::endl;
                is_restoring_ = false;
                return;
            } else {
                std::cerr << "Failed to load plugin from: " << candidate << std::endl;
                // If we copied a backup and it failed to load, clean it up
                if (candidate.extension() == ".backup" && std::filesystem::exists(target_path)) {
                    try {
                        std::filesystem::remove(target_path);
                    } catch (...) {}
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing candidate " << candidate << ": " << e.what() << std::endl;
        }
    }

    std::cerr << "Failed to restore from any available files" << std::endl;
    is_restoring_ = false;
}

void PluginManager::onNewPlugin(const std::filesystem::path& path) {
    // Ignore events if we're currently restoring
    if (is_restoring_) {
        return;
    }

    auto abs_path = std::filesystem::absolute(path);
    std::cout << "New plugin detected: " << path << std::endl;
    
    // Add a small delay to ensure the file is fully written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to load the plugin first to check its type and path
    auto temp_plugin = loader_->loadPlugin(abs_path);
    if (!temp_plugin) {
        std::cout << "Failed to load plugin for inspection" << std::endl;
        return;
    }

    // Check if it's an endpoint plugin
    auto endpoint_plugin = std::dynamic_pointer_cast<plugins::endpoint::EndpointPlugin>(temp_plugin);
    if (!endpoint_plugin) {
        std::cout << "Plugin is not an endpoint plugin" << std::endl;
        return;
    }

    // Check if we already have a plugin with this path and method
    {
        std::lock_guard<std::mutex> lock(plugins_mutex_);
        for (const auto& [existing_path, existing_plugin] : plugins_) {
            auto existing_endpoint = std::dynamic_pointer_cast<plugins::endpoint::EndpointPlugin>(existing_plugin);
            if (existing_endpoint && 
                existing_endpoint->getPath() == endpoint_plugin->getPath() &&
                existing_endpoint->getMethod() == endpoint_plugin->getMethod()) {
                std::cout << "Ignoring new plugin as an endpoint with path " << endpoint_plugin->getPath() 
                         << " and method " << endpoint_plugin->getMethod() << " is already loaded" << std::endl;
                return;
            }
        }
    }

    // If we get here, this is a new unique endpoint
    if (loadPluginWithTimeout(abs_path, false)) {
        manageBackups(abs_path);
    }
}

void PluginManager::onModifiedPlugin(const std::filesystem::path& path) {
    // Ignore events if we're currently restoring
    if (is_restoring_) {
        return;
    }

    auto abs_path = std::filesystem::absolute(path);
    std::cout << "Plugin modified: " << path << std::endl;
    
    // Create backup before modification
    manageBackups(abs_path);

    // Unload existing plugin
    if (unloadPluginWithTimeout(abs_path.string())) {
        // Small delay to ensure file is fully written
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Load new version
        if (!loadPluginWithTimeout(abs_path)) {
            std::cout << "Failed to load modified plugin, attempting restore from backup..." << std::endl;
            restoreFromBackup();
        }
    }
}

std::string PluginManager::getBaseName(const std::filesystem::path& path) const {
    std::string base_name = path.stem().string();
    if (path.extension() == ".backup") {
        base_name = base_name.substr(0, base_name.find_last_of('.'));  // Remove .so
    }
    return base_name.substr(0, base_name.find_last_of('_'));  // Remove timestamp
}

void PluginManager::processPendingDelete(const std::string& base_name, const PendingDelete& pending) {
    std::cout << "Processing deletion for base name: " << base_name << std::endl;
    
    // Get all viable files for this plugin type
    std::vector<std::filesystem::path> so_files;
    std::vector<std::filesystem::path> backup_files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(plugin_directory_)) {
            if (entry.path() != pending.so_path && entry.path() != pending.backup_path) {
                std::string entry_base = getBaseName(entry.path());
                if (entry_base == base_name) {
                    // Verify file exists and is readable
                    try {
                        if (std::filesystem::exists(entry.path()) && 
                            std::filesystem::file_size(entry.path()) > 0) {
                            if (entry.path().extension() == ".so") {
                                so_files.push_back(entry.path());
                            } else if (entry.path().extension() == ".backup" && 
                                     entry.path().stem().extension() == ".so") {
                                backup_files.push_back(entry.path());
                            }
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        std::cerr << "Error checking file " << entry.path() 
                                 << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }

    // Sort both vectors by modification time (newest first)
    auto sort_by_time = [](const auto& a, const auto& b) {
        try {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        } catch (const std::filesystem::filesystem_error&) {
            return false;
        }
    };
    
    std::sort(so_files.begin(), so_files.end(), sort_by_time);
    std::sort(backup_files.begin(), backup_files.end(), sort_by_time);

    // Try restoration in this order:
    // 1. Previous .so file (if exists) - load it directly
    // 2. Current .so.backup (if exists)
    // 3. Previous .so.backup converted to .so
    
    // First try: load previous .so file directly
    if (!so_files.empty()) {
        std::cout << "Loading previous .so: " << so_files.front() << std::endl;
        if (loadPluginWithTimeout(so_files.front())) {
            std::cout << "Successfully loaded previous .so" << std::endl;
            return;
        }
        std::cerr << "Failed to load previous .so" << std::endl;
    }

    // Second try: restore from any available backup
    for (const auto& backup : backup_files) {
        std::cout << "Attempting to restore from backup: " << backup << std::endl;
        try {
            // Get the original .so path by removing .backup extension
            std::filesystem::path restore_path = backup;
            restore_path.replace_extension("");  // Remove .backup extension
            
            // Copy the file
            std::filesystem::copy_file(backup, restore_path,
                                     std::filesystem::copy_options::overwrite_existing);
                
            std::cout << "Successfully restored to: " << restore_path << std::endl;
            
            // Add a delay to ensure file operations are complete
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            if (loadPluginWithTimeout(restore_path)) {
                std::cout << "Successfully loaded restored plugin" << std::endl;
                return;
            }
            
            std::cerr << "Failed to load restored plugin" << std::endl;
            try {
                std::filesystem::remove(restore_path);
            } catch (...) {}
            
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error restoring from backup: " << e.what() << std::endl;
            continue;
        }
    }
    
    std::cerr << "Failed to restore from any available files" << std::endl;
}

void PluginManager::handleBatchedDeletions(const std::string& base_name) {
    std::lock_guard<std::mutex> lock(pending_deletes_mutex_);
    
    auto it = pending_deletes_.find(base_name);
    if (it == pending_deletes_.end()) {
        return;
    }
    
    const auto& pending = it->second;
    auto elapsed = std::chrono::steady_clock::now() - pending.timestamp;
    
    // If not enough time has elapsed, schedule another check
    if (elapsed < DELETION_BATCH_TIMEOUT) {
        std::thread([this, base_name]() {
            std::this_thread::sleep_for(DELETION_BATCH_TIMEOUT);
            handleBatchedDeletions(base_name);
        }).detach();
        return;
    }
    
    // Process the deletion batch
    processPendingDelete(base_name, pending);
    
    // Clean up
    pending_deletes_.erase(it);
}

void PluginManager::onDeletedPlugin(const std::filesystem::path& path) {
    // Ignore events if we're currently restoring
    if (is_restoring_) {
        return;
    }

    auto abs_path = std::filesystem::absolute(path);
    std::cout << "Plugin deleted: " << path << std::endl;
    
    std::string base_name = getBaseName(abs_path);
    bool is_backup = abs_path.extension() == ".backup";
    
    // Update or create pending delete entry
    {
        std::lock_guard<std::mutex> lock(pending_deletes_mutex_);
        auto& pending = pending_deletes_[base_name];
        
        // If this is the first delete for this base name
        if (pending.timestamp == std::chrono::steady_clock::time_point()) {
            pending.timestamp = std::chrono::steady_clock::now();
            pending.so_path = abs_path;
            if (is_backup) {
                pending.so_path.replace_extension("");  // Remove .backup
            }
            pending.backup_path = pending.so_path;
            pending.backup_path += ".backup";
        }
        
        // Mark which file was deleted
        if (is_backup) {
            pending.backup_deleted = true;
        } else {
            pending.so_deleted = true;
        }
        
        // Unload the plugin if it was loaded
        if (!is_backup) {
            bool was_loaded = false;
            {
                std::lock_guard<std::mutex> plugins_lock(plugins_mutex_);
                was_loaded = plugins_.find(abs_path.string()) != plugins_.end();
            }
            
            if (was_loaded && unloadPluginWithTimeout(abs_path.string())) {
                std::cout << "Successfully unloaded deleted plugin" << std::endl;
            }
        }
    }
    
    // Start the batched deletion handler
    std::thread([this, base_name]() {
        handleBatchedDeletions(base_name);
    }).detach();
}

void PluginManager::onPluginWriteComplete(const std::filesystem::path& path) {
    // Ignore events if we're currently restoring
    if (is_restoring_) {
        return;
    }

    auto abs_path = std::filesystem::absolute(path);
    
    // Use a static map to track last modification times
    static std::map<std::filesystem::path, std::chrono::steady_clock::time_point> last_mod_times;
    static std::mutex mod_times_mutex;
    
    // Implement debouncing with a longer timeout and track operation type
    {
        std::lock_guard<std::mutex> lock(mod_times_mutex);
        auto now = std::chrono::steady_clock::now();
        auto& last_time = last_mod_times[abs_path];
        
        if (last_time != std::chrono::steady_clock::time_point() &&
            (now - last_time) < std::chrono::milliseconds(10000)) {  // Increased to 10 seconds
            // Event occurred too soon after the last one, ignore it
            std::cout << "Debouncing: ignoring event for " << path << std::endl;
            return;
        }
        last_time = now;
        
        // Clean up old modification time entries while we have the lock
        auto cleanup_time = now - std::chrono::seconds(60);
        for (auto it = last_mod_times.begin(); it != last_mod_times.end();) {
            if (it->second < cleanup_time) {
                it = last_mod_times.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Add a flag to track if this is a restored file
    bool is_restored = false;
    {
        std::lock_guard<std::mutex> lock(backup_mutex_);
        // Check both the restoration flag and if this is a recently restored file
        is_restored = is_restoring_ || 
                     (std::find_if(backup_files_.begin(), backup_files_.end(),
                         [&abs_path](const auto& backup) {
                             return backup.stem() == abs_path.filename();
                         }) != backup_files_.end());
        
        if (is_restored) {
            std::cout << "Ignoring write event for restored file: " << path << std::endl;
            return;
        }
    }

    // Add a longer delay before attempting to load
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Only do basic existence check before attempting to load
    if (!std::filesystem::exists(abs_path)) {
        std::cerr << "Plugin file does not exist: " << abs_path << std::endl;
        return;
    }
    
    // Try to load the plugin first to check its type and path
    std::shared_ptr<Plugin> temp_plugin;
    try {
        temp_plugin = loader_->loadPlugin(abs_path);
        if (!temp_plugin) {
            std::cout << "Failed to load plugin for inspection" << std::endl;
            return;
        }
    } catch (const std::exception& e) {
        // Only log non-permission errors
        if (std::string(e.what()).find("Permission denied") == std::string::npos) {
            std::cerr << "Error loading plugin for inspection: " << e.what() << std::endl;
        }
        return;
    }

    // Check if it's an endpoint plugin
    auto endpoint_plugin = std::dynamic_pointer_cast<plugins::endpoint::EndpointPlugin>(temp_plugin);
    if (!endpoint_plugin) {
        std::cout << "Plugin is not an endpoint plugin" << std::endl;
        return;
    }

    // Check if we already have a plugin with this path and method
    bool should_replace = false;
    std::string existing_path;
    {
        std::lock_guard<std::mutex> lock(plugins_mutex_);
        for (const auto& [existing_path_str, existing_plugin] : plugins_) {
            auto existing_endpoint = std::dynamic_pointer_cast<plugins::endpoint::EndpointPlugin>(existing_plugin);
            if (existing_endpoint && 
                existing_endpoint->getPath() == endpoint_plugin->getPath() &&
                existing_endpoint->getMethod() == endpoint_plugin->getMethod()) {
                
                // Compare timestamps with higher precision
                try {
                    auto new_time = std::filesystem::last_write_time(abs_path);
                    auto existing_time = std::filesystem::last_write_time(existing_path_str);
                    
                    // Convert to duration since epoch for more precise comparison
                    auto new_duration = new_time.time_since_epoch();
                    auto existing_duration = existing_time.time_since_epoch();
                    
                    if (new_duration > existing_duration) {
                        should_replace = true;
                        existing_path = existing_path_str;
                        std::cout << "New plugin is newer than existing plugin" << std::endl;
                    } else {
                        std::cout << "Ignoring older or same age plugin" << std::endl;
                        return;
                    }
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error comparing plugin timestamps: " << e.what() << std::endl;
                    return;
                }
                break;
            }
        }
    }

    if (should_replace) {
        std::cout << "Replacing existing plugin with newer version for path " << endpoint_plugin->getPath() 
                 << " and method " << endpoint_plugin->getMethod() << std::endl;
        
        // Create backup before unloading the old plugin
        if (!is_restored) {
            manageBackups(abs_path);
        }
        
        // Unload the old plugin first
        if (!unloadPluginWithTimeout(existing_path)) {
            std::cerr << "Failed to unload existing plugin" << std::endl;
            return;
        }
        
        // Load the new plugin
        if (!loadPluginWithTimeout(abs_path)) {
            std::cout << "Failed to load new plugin version, attempting restore from backup..." << std::endl;
            restoreFromBackup();
        }
    } else if (!existing_path.empty()) {
        std::cout << "Keeping existing plugin as it is newer" << std::endl;
    } else {
        // This is a new unique endpoint
        if (loadPluginWithTimeout(abs_path)) {
            // Create backup for new plugins
            if (!is_restored) {
                manageBackups(abs_path);
            }
        }
    }
    
    // Clean up old modification time entries
    {
        std::lock_guard<std::mutex> lock(mod_times_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = last_mod_times.begin(); it != last_mod_times.end();) {
            if (now - it->second > std::chrono::seconds(60)) {
                it = last_mod_times.erase(it);
            } else {
                ++it;
            }
        }
    }
}

std::vector<std::filesystem::path> PluginManager::getBackupFiles() const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    return std::vector<std::filesystem::path>(backup_files_.begin(), backup_files_.end());
}

} // namespace core 