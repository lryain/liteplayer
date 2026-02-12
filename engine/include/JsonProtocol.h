// JsonProtocol.h
// JSON消息协议定义

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

using json = nlohmann::json;

namespace music_player {

/**
 * @brief 命令请求消息
 */
struct CommandRequest {
    std::string command;      // 命令名称
    json params;              // 命令参数
    std::string request_id;   // 请求ID
    
    // 从JSON解析
    static CommandRequest fromJson(const std::string& json_str);
    
    // 转换为JSON
    std::string toJson() const;
};

/**
 * @brief 命令响应消息
 */
struct CommandResponse {
    std::string status;       // success, error
    json result;              // 结果数据
    std::string error_message; // 错误消息（如果status=error）
    std::string request_id;   // 请求ID
    
    // 转换为JSON
    std::string toJson() const;
    
    // 创建成功响应
    static CommandResponse success(const json& result, const std::string& request_id = "");
    
    // 创建错误响应
    static CommandResponse error(const std::string& message, const std::string& request_id = "");
};

/**
 * @brief 事件消息
 */
struct EventMessage {
    std::string event;        // 事件类型
    json data;                // 事件数据
    int64_t timestamp;        // 时间戳
    
    // 转换为JSON
    std::string toJson() const;
    
    // 创建事件消息
    static EventMessage create(const std::string& event_type, const json& event_data);
};

/**
 * @brief 播放状态
 */
enum class PlaybackState {
    STOPPED,
    PLAYING,
    PAUSED
};

// 将PlaybackState转换为字符串
std::string playbackStateToString(PlaybackState state);

// 从字符串转换为PlaybackState
PlaybackState stringToPlaybackState(const std::string& str);

} // namespace music_player
