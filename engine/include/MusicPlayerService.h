// MusicPlayerService.h
// 音乐播放器ZMQ服务

#pragma once

#include "ConfigLoader.h"
#include "JsonProtocol.h"
#include "MusicLibrary.h"
#include "PlaybackController.h"
#include <zmq.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

namespace music_player {

/**
 * @brief 音乐播放器ZMQ服务
 */
class MusicPlayerService {
public:
    MusicPlayerService();
    ~MusicPlayerService();
    
    // 初始化服务
    bool initialize(const std::string& config_file);
    
    // 启动服务
    bool start();
    
    // 停止服务
    void stop();
    
    // 检查是否正在运行
    bool isRunning() const { return running_; }

private:
    // 命令处理循环
    void commandLoop();
    
    // 事件发布循环
    void eventLoop();

    // 处理 PlaybackController 事件（从回调线程转发到 commandLoop 线程）
    void processPendingControllerEvents();
    
    // 处理单个命令
    CommandResponse handleCommand(const CommandRequest& request);
    
    // 发布事件
    void publishEvent(const std::string& event_type, const json& data);
    
    // 命令处理器
    CommandResponse handlePlay(const json& params);
    CommandResponse handlePause(const json& params);
    CommandResponse handleStop(const json& params);
    CommandResponse handleNext(const json& params);
    CommandResponse handlePrevious(const json& params);
    CommandResponse handleGetStatus(const json& params);
    CommandResponse handleGetTrack(const json& params);
    CommandResponse handleSearchTracks(const json& params);
    CommandResponse handleAddTrack(const json& params);
    CommandResponse handleGetAllTracks(const json& params);
    
    // 辅助方法
    bool syncDatabaseTracksToPlaylist();
    bool scanMusicDirectories();
    std::vector<Track> scanDirectory(const std::string& directory);
    
    // ZMQ上下文和socket
    std::unique_ptr<zmq::context_t> zmq_context_;
    std::unique_ptr<zmq::socket_t> cmd_socket_;
    std::unique_ptr<zmq::socket_t> event_socket_;
    
    // 核心组件
    std::unique_ptr<MusicLibrary> library_;
    std::unique_ptr<PlaybackController> controller_;

    // PlaybackController 事件队列（回调线程入队，commandLoop 出队）
    std::mutex event_queue_mutex_;
    std::queue<std::pair<PlayerEvent, std::string>> pending_events_;
    
    // 配置
    MusicPlayerConfig config_;
    
    // 线程
    std::thread cmd_thread_;
    std::thread event_thread_;
    
    // 运行状态
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
};

} // namespace music_player
