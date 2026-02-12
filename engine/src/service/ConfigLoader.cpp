// ConfigLoader.cpp
// 配置文件加载器实现

#include "ConfigLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

namespace fs = std::filesystem;

namespace music_player {

// 将相对路径转换为绝对路径
static std::string resolvePath(const std::string& path) {
    // 如果已经是绝对路径，直接返回
    if (path.empty() || path[0] == '/' || path[0] == '~') {
        if (path[0] == '~') {
            // 处理 ~ 开头的路径
            const char* home = std::getenv("HOME");
            if (home) {
                return std::string(home) + path.substr(1);
            }
        }
        return path;
    }
    
    // 相对路径：相对于项目根目录 (3rd/liteplayer)
    // 获取当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        fs::path base(cwd);
        fs::path result = base / path;
        // 规范化路径（消除 .. 和 . 而不需要检查文件存在）
        // 使用 weakly_canonical 而不是 canonical（不需要路径存在）
        try {
            return fs::weakly_canonical(result).string();
        } catch (...) {
            return result.string();
        }
    }
    
    return path;  // 返回原始路径作为后备
}

ConfigLoader& ConfigLoader::getInstance() {
    static ConfigLoader instance;
    return instance;
}

std::string ConfigLoader::getDefaultConfigPath() {
    // 默认配置路径：优先使用 liteplayer/config 下的配置。
    // 历史上用户可能会传入 config/config.yaml（不存在）或 config/music_player.yaml。
    // 这里做一个尽量稳健的 fallback。
    const std::vector<std::string> candidates = {
        "config/music_player.yaml",
        "config/music_player.yaml", // 保留占位，便于未来扩展
        "config/config.yaml",
    };

    for (const auto& p : candidates) {
        try {
            if (fs::exists(p)) return p;
        } catch (...) {
            // ignore
        }
    }
    return "config/music_player.yaml";
}

bool ConfigLoader::load(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[ConfigLoader] Failed to open config file: " << config_file << std::endl;
        // 给出常见候选路径提示，方便排障
        std::cerr << "[ConfigLoader] Hint: expected something like: config/music_player.yaml" << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    if (!parseConfig(content)) {
        std::cerr << "[ConfigLoader] Failed to parse config file" << std::endl;
        return false;
    }
    
    loaded_ = true;
    std::cout << "[ConfigLoader] Configuration loaded successfully" << std::endl;
    return true;
}

// 简化的YAML解析（支持嵌套结构）
bool ConfigLoader::parseConfig(const std::string& content) {
    // 设置默认值
    config_.zmq.command_endpoint = "ipc:///tmp/music_player_cmd.sock";
    config_.zmq.event_endpoint = "ipc:///tmp/music_player_event.sock";
    
    config_.database.path = "data/music_library.db";  // 默认使用相对路径
    config_.database.test_path = "/tmp/test_music.db";
    config_.database.backup_dir = "data/backups";
    config_.database.auto_backup_enabled = true;
    config_.database.backup_interval_days = 7;
    config_.database.max_backups = 5;
    
    config_.player.default_play_mode = "sequential";
    config_.player.audio_output.device = "default";
    config_.player.audio_output.buffer_size = 4096;
    config_.player.progress_update_interval = 1000;
    config_.player.preload_next_track = true;
    
    config_.service.name = "music-player";
    config_.service.pid_file = "/var/run/music-player.pid";
    config_.service.user = "pi";
    config_.service.group = "pi";
    
    config_.scan_directories.clear();  // 清空默认值，以便从配置文件读取
    config_.scan_directories.push_back("../../assets/sounds/animal");
    config_.scan_directories.push_back("../../assets/sounds/music");
    
    config_.supported_formats.push_back("mp3");
    config_.supported_formats.push_back("m4a");
    config_.supported_formats.push_back("aac");
    config_.supported_formats.push_back("wav");
    
    // 解析YAML - 支持嵌套结构
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    std::string current_subsection;
    int current_indent = 0;
    bool in_scan_directories = false;
    bool in_supported_formats = false;
    
    while (std::getline(stream, line)) {
        if (line.empty() || (line[0] == '#')) continue;
        
        // 计算缩进级别
        int indent = 0;
        size_t pos = 0;
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
            indent += (line[pos] == '\t') ? 4 : 1;
            pos++;
        }
        
        // 提取内容
        std::string content_line = line.substr(pos);
        if (content_line.empty() || content_line[0] == '#') continue;
        
        // 处理section和值
        size_t colon_pos = content_line.find(':');
        if (colon_pos == std::string::npos) {
            // 可能是列表项 (以 - 开头)
            if (content_line[0] == '-') {
                std::string item = content_line.substr(1);
                // 清理项
                item.erase(0, item.find_first_not_of(" \t\""));
                item.erase(item.find_last_not_of(" \t\"") + 1);
                
                if (in_scan_directories) {
                    config_.scan_directories.push_back(resolvePath(item));
                } else if (in_supported_formats) {
                    config_.supported_formats.push_back(item);
                }
            }
            continue;
        }
        
        std::string key = content_line.substr(0, colon_pos);
        std::string value = content_line.substr(colon_pos + 1);
        
        // 清理键和值
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);
        
        // 更新section信息
        if (indent == 0) {
            current_section = key;
            current_subsection = "";
            in_scan_directories = false;
            in_supported_formats = false;
        } else if (indent == 2 || indent == 4) {
            // 检查是否是列表section
            if (value.empty()) {
                current_subsection = key;
                if (key == "scan_directories") {
                    in_scan_directories = true;
                    config_.scan_directories.clear();  // 清空默认值
                } else if (key == "supported_formats") {
                    in_supported_formats = true;
                    config_.supported_formats.clear();  // 清空默认值
                } else {
                    in_scan_directories = false;
                    in_supported_formats = false;
                }
            } else {
                // 这是一个值
                in_scan_directories = false;
                in_supported_formats = false;
                
                // 特殊处理database.path
                if (current_section == "library" && current_subsection == "database" && key == "path") {
                    config_.database.path = resolvePath(value);
                    std::cout << "[ConfigLoader] Database path: " << value << " -> " 
                              << config_.database.path << std::endl;
                }
                else if (current_section == "zmq") {
                    if (key == "command_endpoint") config_.zmq.command_endpoint = value;
                    else if (key == "event_endpoint") config_.zmq.event_endpoint = value;
                }
            }
        }
    }
    
    return true;
}

} // namespace music_player
