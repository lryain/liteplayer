// PlaybackController.h
// 播放控制器 - 整合LitePlayerWrapper和PlaylistManager提供完整的播放控制

#pragma once

#include "LitePlayerWrapper.h"
#include "PlaylistManager.h"
#include <memory>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace music_player {

// 播放控制器事件类型
enum class PlayerEvent {
    TrackStarted,      // 曲目开始播放
    TrackEnded,        // 曲目播放结束
    PlaylistEnded,     // 播放列表结束
    ErrorOccurred,     // 发生错误
    StateChanged       // 状态变化
};

// 事件回调
using EventCallback = std::function<void(PlayerEvent event, const std::string& info)>;

class PlaybackController {
public:
    PlaybackController();
    ~PlaybackController() = default;

    // 初始化
    bool initialize();
    
    // 播放列表管理
    bool loadPlaylist(const std::string& path);  // 加载目录或文件
    void setPlayMode(PlayMode mode);
    PlayMode getPlayMode() const;
    
    // 播放控制
    bool play();           // 播放当前曲目
    bool playTrack(size_t index);  // 播放指定曲目
    bool pause();          // 暂停
    bool resume();         // 恢复
    bool stop();           // 停止
    bool next();           // 下一首（自动播放）
    bool prev();           // 上一首（自动播放）
    bool seek(int positionMs);  // 跳转位置
    
    // 状态查询
    PlayState getState() const;
    Track getCurrentTrack() const;
    size_t getCurrentTrackIndex() const;
    size_t getPlaylistSize() const;
    int getPosition() const;     // 当前位置（毫秒）
    int getDuration() const;     // 总时长（毫秒）
    
    // 播放列表访问（用于高级操作）
    PlaylistManager& getPlaylistManager() { return playlist_; }
    
    // 事件回调
    void setEventCallback(EventCallback callback);

private:
    // 状态处理（由回调线程调用）
    void onPlayerStateChanged(PlayState newState);
    void handleError(const std::string& error);
    
    // 播放流程（由命令线程调用）
    bool startCurrentTrack();
    
    /**
     * @brief 内部启动逻辑（调用时必须持有锁，内部会临时释放）
     */
    bool startCurrentTrackInternal(std::unique_lock<std::mutex>& lock);
    
    /**
     * @brief 安全停止流程（调用时必须持有锁，内部会临时释放）
     */
    bool safeStopInternal(std::unique_lock<std::mutex>& lock);
    
    LitePlayerWrapper player_;       // liteplayer包装器
    PlaylistManager playlist_;       // 播放列表管理器
    PlayState currentState_;         // 当前状态
    EventCallback eventCallback_;    // 事件回调
    
    bool autoPlayNext_;              // 播放完成后自动播放下一首
    bool isTransitioning_;           // 是否正在进行曲目切换（手动发起）
    int errorRetryCount_;            // 错误重试计数
    static constexpr int MAX_RETRIES = 3;
    
    mutable std::mutex mutex_;            // 状态锁（保护状态变量）
    mutable std::mutex playerOpMutex_;    // 播放器操作锁（保护player_对象的调用）
    std::condition_variable cvState_;     // 状态变化通知变量
};

} // namespace music_player
