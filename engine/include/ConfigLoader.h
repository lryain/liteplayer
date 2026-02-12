// ConfigLoader.h
// 配置文件加载器

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace music_player {

// 数据库配置
struct DatabaseConfig {
    std::string path;
    std::string test_path;
    std::string backup_dir;
    bool auto_backup_enabled;
    int backup_interval_days;
    int max_backups;
};

// ZMQ配置
struct ZmqConfig {
    std::string command_endpoint;
    std::string event_endpoint;
};

// 音频输出配置
struct AudioConfig {
    std::string device;
    int buffer_size;
};

// 播放器配置
struct PlayerConfig {
    std::string default_play_mode;
    AudioConfig audio_output;
    int progress_update_interval;
    bool preload_next_track;
};

// 服务配置
struct ServiceConfig {
    std::string name;
    std::string pid_file;
    std::string user;
    std::string group;
};

// 完整配置
struct MusicPlayerConfig {
    ZmqConfig zmq;
    DatabaseConfig database;
    PlayerConfig player;
    ServiceConfig service;
    std::vector<std::string> scan_directories;
    std::vector<std::string> supported_formats;
};

/**
 * @brief 配置加载器（单例模式）
 */
class ConfigLoader {
public:
    // 获取单例实例
    static ConfigLoader& getInstance();
    
    // 加载配置文件
    bool load(const std::string& config_file);
    
    // 获取配置
    const MusicPlayerConfig& getConfig() const { return config_; }
    
    // 检查是否已加载
    bool isLoaded() const { return loaded_; }
    
    // 获取默认配置文件路径
    static std::string getDefaultConfigPath();

private:
    ConfigLoader() : loaded_(false) {}
    ~ConfigLoader() = default;
    
    // 禁止拷贝和赋值
    ConfigLoader(const ConfigLoader&) = delete;
    ConfigLoader& operator=(const ConfigLoader&) = delete;
    
    // 解析配置文件（简化版，不依赖yaml-cpp）
    bool parseConfig(const std::string& content);
    
    MusicPlayerConfig config_;
    bool loaded_;
};

} // namespace music_player
