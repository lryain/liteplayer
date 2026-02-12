// JsonProtocol.cpp
// JSON消息协议实现

#include "JsonProtocol.h"
#include <chrono>
#include <iostream>

namespace music_player {

// ========== CommandRequest ==========

CommandRequest CommandRequest::fromJson(const std::string& json_str) {
    CommandRequest req;
    try {
        json j = json::parse(json_str);
        req.command = j.value("command", "");
        req.params = j.value("params", json::object());
        req.request_id = j.value("request_id", "");
    } catch (const std::exception& e) {
        std::cerr << "[JsonProtocol] Failed to parse command request: " << e.what() << std::endl;
    }
    return req;
}

std::string CommandRequest::toJson() const {
    json j;
    j["command"] = command;
    j["params"] = params;
    j["request_id"] = request_id;
    return j.dump();
}

// ========== CommandResponse ==========

std::string CommandResponse::toJson() const {
    json j;
    j["status"] = status;
    j["result"] = result;
    if (!error_message.empty()) {
        j["error_message"] = error_message;
    }
    j["request_id"] = request_id;
    return j.dump();
}

CommandResponse CommandResponse::success(const json& result, const std::string& request_id) {
    CommandResponse resp;
    resp.status = "success";
    resp.result = result;
    resp.request_id = request_id;
    return resp;
}

CommandResponse CommandResponse::error(const std::string& message, const std::string& request_id) {
    CommandResponse resp;
    resp.status = "error";
    resp.error_message = message;
    resp.result = json::object();
    resp.request_id = request_id;
    return resp;
}

// ========== EventMessage ==========

std::string EventMessage::toJson() const {
    json j;
    j["event"] = event;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j.dump();
}

EventMessage EventMessage::create(const std::string& event_type, const json& event_data) {
    EventMessage msg;
    msg.event = event_type;
    msg.data = event_data;
    
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    msg.timestamp = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    return msg;
}

// ========== PlaybackState ==========

std::string playbackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::STOPPED: return "stopped";
        case PlaybackState::PLAYING: return "playing";
        case PlaybackState::PAUSED: return "paused";
        default: return "unknown";
    }
}

PlaybackState stringToPlaybackState(const std::string& str) {
    if (str == "playing") return PlaybackState::PLAYING;
    if (str == "paused") return PlaybackState::PAUSED;
    return PlaybackState::STOPPED;
}

} // namespace music_player
