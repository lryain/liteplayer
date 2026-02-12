// PlaybackController.cpp
// 播放控制器实现

#include "PlaybackController.h"
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <mutex>

namespace music_player {

PlaybackController::PlaybackController()
    : currentState_(PlayState::Idle)
    , autoPlayNext_(true)
    , isTransitioning_(false)
    , errorRetryCount_(0)
{
}

bool PlaybackController::initialize() {
    // 初始化player
    if (!player_.initialize()) {
        std::cerr << "[PlaybackController] Failed to initialize player" << std::endl;
        return false;
    }
    
    // 设置状态回调
    player_.setStateCallback([this](PlayState state, int error_code) {
        onPlayerStateChanged(state);
        if (error_code != 0) {
            handleError("Error code: " + std::to_string(error_code));
        }
    });
    
    std::cout << "[PlaybackController] Initialized successfully" << std::endl;
    return true;
}

bool PlaybackController::loadPlaylist(const std::string& path) {
    // 停止当前播放
    if (currentState_ == PlayState::Playing) {
        stop();
    }
    
    // 加载播放列表
    bool success = false;
    if (std::filesystem::is_directory(path)) {
        success = playlist_.loadFromDirectory(path);
    } else {
        success = playlist_.loadFromFile(path);
    }
    
    if (!success) {
        std::cerr << "[PlaybackController] Failed to load playlist from: " << path << std::endl;
        return false;
    }
    
    std::cout << "[PlaybackController] Loaded playlist: " << playlist_.getTrackCount() 
              << " tracks" << std::endl;
    
    return true;
}

void PlaybackController::setPlayMode(PlayMode mode) {
    playlist_.setPlayMode(mode);
    std::cout << "[PlaybackController] Play mode set to: " << static_cast<int>(mode) << std::endl;
}

PlayMode PlaybackController::getPlayMode() const {
    return playlist_.getPlayMode();
}

bool PlaybackController::play() {
    std::cout << "[PlaybackController::play] ========== START ==========" << std::endl;
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (currentState_ == PlayState::Playing) {
        std::cout << "[PlaybackController::play] Already playing" << std::endl;
        return true;
    }
    
    if (playlist_.isEmpty()) {
        std::cerr << "[PlaybackController] Playlist is empty" << std::endl;
        return false;
    }
    
    if (currentState_ == PlayState::Paused) {
        std::cout << "[PlaybackController::play] Resuming from pause" << std::endl;
        lock.unlock();
        return resume();
    }
    
    // 播放当前曲目
    std::cout << "[PlaybackController::play] Starting current track" << std::endl;
    bool result = startCurrentTrackInternal(lock);
    
    std::cout << "[PlaybackController::play] ========== END ==========" << std::endl;
    return result;
}

bool PlaybackController::playTrack(size_t index) {
    std::cout << "[PlaybackController::playTrack] ========== START (index=" << index << ") ==========" << std::endl;
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!playlist_.seekTo(index)) {
        std::cerr << "[PlaybackController] Invalid track index: " << index << std::endl;
        return false;
    }
    
    std::cout << "[PlaybackController::playTrack] Seeking to index " << index << std::endl;
    
    // 如果当前正在播放，需要先停止
    if (currentState_ == PlayState::Playing || currentState_ == PlayState::Paused) {
        std::cout << "[PlaybackController::playTrack] Stopping current playback before switching" << std::endl;
        isTransitioning_ = true;
        safeStopInternal(lock);
    }
    
    std::cout << "[PlaybackController::playTrack] Starting track at index " << index << std::endl;
    bool result = startCurrentTrackInternal(lock);
    
    if (currentState_ != PlayState::Idle && currentState_ != PlayState::Stopped) {
        isTransitioning_ = false;
    }
    
    std::cout << "[PlaybackController::playTrack] ========== END (result=" << result << ") ==========" << std::endl;
    return result;
}

bool PlaybackController::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentState_ != PlayState::Playing) {
        std::cout << "[PlaybackController] Not playing, cannot pause" << std::endl;
        return false;
    }
    
    bool success = player_.pause();
    if (success) {
        std::cout << "[PlaybackController] Paused" << std::endl;
    }
    return success;
}

bool PlaybackController::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentState_ != PlayState::Paused) {
        std::cout << "[PlaybackController] Not paused, cannot resume" << std::endl;
        return false;
    }
    
    bool success = player_.resume();
    if (success) {
        std::cout << "[PlaybackController] Resumed" << std::endl;
    }
    return success;
}

bool PlaybackController::stop() {
    std::cout << "[PlaybackController::stop] ========== START ==========" << std::endl;
    
    std::unique_lock<std::mutex> lock(mutex_);
    isTransitioning_ = true;
    
    bool result = safeStopInternal(lock);
    
    isTransitioning_ = false;
    std::cout << "[PlaybackController::stop] ========== END ==========" << std::endl;
    
    return result;
}

bool PlaybackController::next() {
    std::cout << "[PlaybackController::next] ========== START ==========" << std::endl;
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!playlist_.hasNext()) {
        std::cout << "[PlaybackController] No next track available" << std::endl;
        lock.unlock();
        if (eventCallback_) {
            eventCallback_(PlayerEvent::PlaylistEnded, "Reached end of playlist");
        }
        return false;
    }
    
    std::cout << "[PlaybackController::next] Setting isTransitioning_=true" << std::endl;
    isTransitioning_ = true;
    
    // 安全停止当前播放
    std::cout << "[PlaybackController::next] Calling safeStopInternal..." << std::endl;
    safeStopInternal(lock);
    
    // 移动到下一曲
    std::cout << "[PlaybackController::next] Moving to next track in playlist" << std::endl;
    playlist_.next();
    
    // 开始播放
    std::cout << "[PlaybackController::next] Starting new track..." << std::endl;
    bool result = startCurrentTrackInternal(lock);
    
    isTransitioning_ = false;
    std::cout << "[PlaybackController::next] ========== END (result=" << result << ") ==========" << std::endl;
    
    return result;
}

bool PlaybackController::prev() {
    std::cout << "[PlaybackController::prev] ========== START ==========" << std::endl;
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!playlist_.hasPrev()) {
        std::cout << "[PlaybackController] No previous track available" << std::endl;
        return false;
    }
    
    std::cout << "[PlaybackController::prev] Setting isTransitioning_=true" << std::endl;
    isTransitioning_ = true;
    
    // 安全停止当前播放
    std::cout << "[PlaybackController::prev] Calling safeStopInternal..." << std::endl;
    safeStopInternal(lock);
    
    // 移动到上一曲
    std::cout << "[PlaybackController::prev] Moving to previous track in playlist" << std::endl;
    playlist_.prev();
    
    // 开始播放
    std::cout << "[PlaybackController::prev] Starting new track..." << std::endl;
    bool result = startCurrentTrackInternal(lock);
    
    isTransitioning_ = false;
    std::cout << "[PlaybackController::prev] ========== END (result=" << result << ") ==========" << std::endl;
    
    return result;
}

bool PlaybackController::safeStopInternal(std::unique_lock<std::mutex>& lock) {
    PlayState state = currentState_;
    
    if (state == PlayState::Idle || state == PlayState::Stopped) {
        std::cout << "[PlaybackController] safeStopInternal: already stopped/idle" << std::endl;
        return true;
    }
    
    std::cout << "[PlaybackController] safeStopInternal: initiating stop from state=" 
              << static_cast<int>(state) << std::endl;
    
    // 释放状态锁，获取播放器锁，调用底层停止
    lock.unlock();
    {
        std::lock_guard<std::mutex> playerLock(playerOpMutex_);
        std::cout << "[PlaybackController] safeStopInternal: calling player_.stop() with player lock" << std::endl;
        player_.stop();
    }
    lock.lock();
    
    std::cout << "[PlaybackController] safeStopInternal: stop called, waiting for Stopped state..." << std::endl;
    
    // 等待状态变为 Stopped (最多等待 3s)
    bool reached = cvState_.wait_for(lock, std::chrono::seconds(3), [this] {
        return currentState_ == PlayState::Stopped || currentState_ == PlayState::Idle;
    });
    
    if (!reached) {
        std::cerr << "[PlaybackController] ⚠️  safeStopInternal: TIMEOUT waiting for Stopped state, current=" 
                  << static_cast<int>(currentState_) << std::endl;
    } else {
        std::cout << "[PlaybackController] safeStopInternal: reached Stopped state" << std::endl;
    }
    
    // Reset player（释放状态锁，获取播放器锁）
    lock.unlock();
    {
        std::lock_guard<std::mutex> playerLock(playerOpMutex_);
        std::cout << "[PlaybackController] safeStopInternal: calling player_.reset() with player lock" << std::endl;
        player_.reset();
        usleep(50000); // 50ms 缓冲
    }
    lock.lock();
    
    std::cout << "[PlaybackController] safeStopInternal: completed" << std::endl;
    return true;
}

bool PlaybackController::seek(int positionMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentState_ != PlayState::Playing && currentState_ != PlayState::Paused) {
        std::cerr << "[PlaybackController] Cannot seek in current state" << std::endl;
        return false;
    }
    
    return player_.seek(positionMs);
}

PlayState PlaybackController::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentState_;
}

Track PlaybackController::getCurrentTrack() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playlist_.getCurrentTrack();
}

size_t PlaybackController::getCurrentTrackIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playlist_.getCurrentIndex();
}

size_t PlaybackController::getPlaylistSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playlist_.getTrackCount();
}

int PlaybackController::getPosition() const {
    return player_.getPosition();
}

int PlaybackController::getDuration() const {
    return player_.getDuration();
}

void PlaybackController::setEventCallback(EventCallback callback) {
    eventCallback_ = callback;
}

void PlaybackController::onPlayerStateChanged(PlayState newState) {
    std::cout << "[PlaybackController::onPlayerStateChanged] Callback thread - new state: " 
              << static_cast<int>(newState) << std::endl;
    
    PlayState oldState;
    bool shouldAutoNext = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        oldState = currentState_;
        currentState_ = newState;
        
        std::cout << "[PlaybackController] State transition: " 
                  << static_cast<int>(oldState) << " -> " 
                  << static_cast<int>(newState) 
                  << " (isTransitioning=" << isTransitioning_ << ")" << std::endl;
        
        // 通知所有等待状态变化的线程
        cvState_.notify_all();
        
        // 检查是否需要自动播放下一首（仅在非手动切换时）
        if (!isTransitioning_ && 
            oldState == PlayState::Playing && 
            newState == PlayState::Stopped &&
            autoPlayNext_) {
            shouldAutoNext = true;
        }
    }
    
    // 在锁外执行回调和业务逻辑
    if (eventCallback_) {
        eventCallback_(PlayerEvent::StateChanged, 
                      "State: " + std::to_string(static_cast<int>(newState)));
    }
    
    // 自动播放下一首：不要在 liteplayer 回调线程里直接调用 next()
    // 原因：next() 会触发 listplayer_set_data_source/prepare_async/reset 等，
    // 这些若在解码/looper 线程回调上下文内调用，可能与 liteplayer 内部锁形成互锁。
    // 改为仅发出 TrackEnded 事件，由上层服务（命令线程）决定是否调用 next()。
    if (shouldAutoNext) {
        std::cout << "[PlaybackController] Track completed (auto-next requested)" << std::endl;
        if (eventCallback_) {
            Track track = getCurrentTrack();
            eventCallback_(PlayerEvent::TrackEnded, track.title);
        }
    }
    
    // 处理错误状态
    if (newState == PlayState::Error) {
        handleError("Playback error occurred");
    }
}

void PlaybackController::handleError(const std::string& error) {
    std::cerr << "[PlaybackController] Error: " << error << std::endl;
    
    // 通知错误
    if (eventCallback_) {
        eventCallback_(PlayerEvent::ErrorOccurred, error);
    }
    
    // 错误恢复：不要在 liteplayer 回调线程里直接调用 next()。
    // 原因同 TrackEnded：next()/reset/load/prepare 可能在 liteplayer 内部线程回调上下文里互锁。
    // 改为仅发事件，由上层服务（命令线程）决定是否切歌。
    bool shouldRequestNext = false;
    bool noMoreTracks = false;
    int retryCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        errorRetryCount_++;
        retryCount = errorRetryCount_;
        shouldRequestNext = (errorRetryCount_ < MAX_RETRIES && playlist_.hasNext());
        noMoreTracks = !playlist_.hasNext();
        if (!shouldRequestNext) {
            errorRetryCount_ = 0;
        }
    }

    if (shouldRequestNext) {
        std::cout << "[PlaybackController] Error recovery: request next track (retry "
                  << retryCount << "/" << MAX_RETRIES << ")" << std::endl;
        if (eventCallback_) {
            // 复用 TrackEnded 事件语义：让服务线程走统一的 next() 路径。
            Track track = getCurrentTrack();
            eventCallback_(PlayerEvent::TrackEnded, track.title);
        }
    } else {
        std::cerr << "[PlaybackController] Error recovery: stop retrying"
                  << (noMoreTracks ? " (no more tracks)" : " (max retries)") << std::endl;
    }
}

bool PlaybackController::startCurrentTrack() {
    std::unique_lock<std::mutex> lock(mutex_);
    return startCurrentTrackInternal(lock);
}

bool PlaybackController::startCurrentTrackInternal(std::unique_lock<std::mutex>& lock) {
    // 调用此函数时必须已持有 unique_lock
    // 获取曲目信息（在锁内）
    Track track = playlist_.getCurrentTrack();
    
    if (track.file_path.empty()) {
        std::cerr << "[PlaybackController] Invalid track (empty path)" << std::endl;
        return false;
    }
    
    std::cout << "[PlaybackController] startCurrentTrackInternal: \"" << track.title 
              << "\" (path=" << track.file_path << ")" << std::endl;
    
    // 释放状态锁，并获取播放器操作锁（互斥保护 player_ 对象）
    std::cout << "[PlaybackController] Releasing state lock, acquiring player lock..." << std::endl;
    lock.unlock();
    std::lock_guard<std::mutex> playerLock(playerOpMutex_);
    std::cout << "[PlaybackController] ✅ Acquired player operation lock" << std::endl;
    
    std::cout << "[PlaybackController] Calling player_.reset()..." << std::endl;
    player_.reset();
    std::cout << "[PlaybackController] Waiting for reset to complete (200ms)..." << std::endl;
    usleep(200000); // 200ms - 等待异步销毁完成
    
    std::cout << "[PlaybackController] Loading file: " << track.file_path << std::endl;
    bool loadResult = player_.loadFile(track.file_path);
    if (!loadResult) {
        std::cerr << "[PlaybackController] ❌ Failed to load: " << track.file_path << std::endl;
        if (eventCallback_) {
            // 用 file_path 作为 info，便于上层做坏轨隔离持久化
            eventCallback_(PlayerEvent::ErrorOccurred, track.file_path);
        }
        lock.lock();  // 重新获取锁再返回
        return false;
    }
    
    std::cout << "[PlaybackController] Waiting for async prepare (500ms)..." << std::endl;
    usleep(500000);  // 500ms - 对 MP3 文件很重要
    
    std::cout << "[PlaybackController] Calling player_.start()..." << std::endl;
    bool startResult = player_.start();
    
    // 重新获取状态锁
    lock.lock();
    
    if (!startResult) {
        std::cerr << "[PlaybackController] ❌ Failed to start playback" << std::endl;
        if (eventCallback_) {
            eventCallback_(PlayerEvent::ErrorOccurred, track.file_path);
        }
        return false;
    }
    
    std::cout << "[PlaybackController] ✅ Track started successfully" << std::endl;
    return true;
}

} // namespace music_player
