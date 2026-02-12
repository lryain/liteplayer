// MusicPlayerService.cpp
// 音乐播放器ZMQ服务实现

#include "MusicPlayerService.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <ctime>
#include <mutex>
#include <queue>

namespace music_player {

MusicPlayerService::MusicPlayerService()
    : running_(false)
    , should_stop_(false)
{
}

MusicPlayerService::~MusicPlayerService() {
    stop();
}

bool MusicPlayerService::initialize(const std::string& config_file) {
    // 加载配置
    auto& config_loader = ConfigLoader::getInstance();
    if (!config_loader.load(config_file)) {
        std::cerr << "[MusicPlayerService] Failed to load config" << std::endl;
        return false;
    }
    config_ = config_loader.getConfig();
    
    // 初始化ZMQ
    try {
        zmq_context_ = std::make_unique<zmq::context_t>(1);
        
        // 命令socket (REP)
        cmd_socket_ = std::make_unique<zmq::socket_t>(*zmq_context_, zmq::socket_type::rep);
        cmd_socket_->bind(config_.zmq.command_endpoint);
        
        // 事件socket (PUB)
        event_socket_ = std::make_unique<zmq::socket_t>(*zmq_context_, zmq::socket_type::pub);
        event_socket_->bind(config_.zmq.event_endpoint);
        
        std::cout << "[MusicPlayerService] ZMQ initialized" << std::endl;
        std::cout << "  Command endpoint: " << config_.zmq.command_endpoint << std::endl;
        std::cout << "  Event endpoint: " << config_.zmq.event_endpoint << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "[MusicPlayerService] ZMQ error: " << e.what() << std::endl;
        return false;
    }
    
    // 初始化音乐库
    library_ = std::make_unique<MusicLibrary>();
    if (!library_->open(config_.database.path)) {
        std::cerr << "[MusicPlayerService] Failed to open music library" << std::endl;
        return false;
    }
    
    // 初始化播放控制器
    controller_ = std::make_unique<PlaybackController>();
    if (!controller_->initialize()) {
        std::cerr << "[MusicPlayerService] Failed to initialize playback controller" << std::endl;
        return false;
    }

    // 将 PlaybackController 的事件回调桥接到服务线程：
    // 禁止在 liteplayer 的回调线程内直接执行 next()/reset/load 等重操作。
    controller_->setEventCallback([this](PlayerEvent event, const std::string& info) {
        // 轻量级：只入队，真正处理在 commandLoop 线程里完成。
        {
            std::lock_guard<std::mutex> lk(event_queue_mutex_);
            pending_events_.emplace(event, info);
        }
    });
    
    // 首次启动时扫描配置目录中的音乐
    std::cout << "[MusicPlayerService] Scanning music directories..." << std::endl;
    scanMusicDirectories();
    
    // 从数据库加载曲目到播放列表（通过临时目录）
    if (!syncDatabaseTracksToPlaylist()) {
        std::cerr << "[MusicPlayerService] Warning: Failed to sync tracks to playlist" << std::endl;
        // 不是致命错误，继续运行
    }
    
    std::cout << "[MusicPlayerService] Service initialized successfully" << std::endl;
    return true;
}

bool MusicPlayerService::start() {
    if (running_) {
        std::cerr << "[MusicPlayerService] Service already running" << std::endl;
        return false;
    }
    
    should_stop_ = false;
    running_ = true;
    
    // 启动命令处理线程
    cmd_thread_ = std::thread(&MusicPlayerService::commandLoop, this);
    
    // 启动事件发布线程
    event_thread_ = std::thread(&MusicPlayerService::eventLoop, this);
    
    std::cout << "[MusicPlayerService] Service started" << std::endl;
    return true;
}

void MusicPlayerService::stop() {
    if (!running_) return;
    
    std::cout << "[MusicPlayerService] Stopping service... (should_stop_ set to true)" << std::endl;
    // 增加占位日志，方便追踪调用者
    static int stop_count = 0;
    stop_count++;
    std::cout << "[MusicPlayerService] stop() called, count: " << stop_count << std::endl;

    should_stop_ = true;
    running_ = false;
    
    // 等待线程结束
    if (cmd_thread_.joinable()) {
        cmd_thread_.join();
    }
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
    
    // 关闭库
    if (library_) {
        library_->close();
    }
    
    std::cout << "[MusicPlayerService] Service stopped" << std::endl;
}

void MusicPlayerService::commandLoop() {
    std::cout << "[MusicPlayerService] Command loop started" << std::endl;
    
    while (!should_stop_) {
        try {
            // 处理从 PlaybackController 回调线程转发来的事件
            processPendingControllerEvents();

            zmq::message_t request;
            
            // 接收命令（带超时）
            auto result = cmd_socket_->recv(request, zmq::recv_flags::dontwait);
            
            if (!result) {
                // 空闲时也处理一次事件队列，避免事件堆积
                processPendingControllerEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // 解析命令
            std::string request_str(static_cast<char*>(request.data()), request.size());
            CommandRequest cmd = CommandRequest::fromJson(request_str);
            
            std::cout << "[MusicPlayerService] Received command: " << cmd.command << std::endl;
            
            // 处理命令
            CommandResponse response = handleCommand(cmd);
            
            // 发送响应
            std::string response_str = response.toJson();
            std::cout << "[MusicPlayerService] Sending response for: " << cmd.command << std::endl;
            zmq::message_t reply(response_str.size());
            memcpy(reply.data(), response_str.c_str(), response_str.size());
            cmd_socket_->send(reply, zmq::send_flags::none);
            std::cout << "[MusicPlayerService] Response sent" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[MusicPlayerService] Command loop error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[MusicPlayerService] Command loop unknown error" << std::endl;
        }
    }
    
    std::cout << "[MusicPlayerService] Command loop stopped" << std::endl;
}

void MusicPlayerService::processPendingControllerEvents() {
    std::queue<std::pair<PlayerEvent, std::string>> local;
    {
        std::lock_guard<std::mutex> lk(event_queue_mutex_);
        if (pending_events_.empty()) return;
        std::swap(local, pending_events_);
    }

    while (!local.empty()) {
        auto [ev, info] = local.front();
        local.pop();

        if (!controller_) continue;

        // 节流：避免坏文件导致 Error/TrackEnded 事件风暴，拖垮 commandLoop。
        static auto last_next_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        auto tryNext = [&]() {
            auto now = std::chrono::steady_clock::now();
            if (now - last_next_time < std::chrono::milliseconds(200)) {
                std::cout << "[MusicPlayerService] Skip next(): throttled" << std::endl;
                return;
            }
            last_next_time = now;
            controller_->next();
        };

        switch (ev) {
            case PlayerEvent::TrackEnded: {
                std::cout << "[MusicPlayerService] Controller event: TrackEnded (" << info << ")" << std::endl;
                // 在 commandLoop 线程中执行 next()，避免在 liteplayer 回调线程里做重操作
                tryNext();
                break;
            }
            case PlayerEvent::ErrorOccurred: {
                std::cerr << "[MusicPlayerService] Controller event: ErrorOccurred (" << info << ")" << std::endl;
                // info 可能是 file_path，也可能是普通错误文案（如 "Playback error occurred"）。
                // 仅在其看起来是有效音频文件路径时才做坏轨隔离，避免误标数据库。
                bool markedBad = false;
                if (library_ && library_->isOpen() && !info.empty()) {
                    namespace fs = std::filesystem;
                    try {
                        fs::path p(info);
                        std::string ext = p.extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        const bool looksAudio = (ext == ".wav" || ext == ".mp3" || ext == ".m4a" || ext == ".aac");
                        if (looksAudio && fs::exists(p)) {
                            markedBad = library_->markTrackBadByPath(info, "liteplayer_load_or_start_failed");
                        } else {
                            std::cerr << "[MusicPlayerService] Skip mark bad track: info is not a valid audio path" << std::endl;
                        }
                    } catch (...) {
                        std::cerr << "[MusicPlayerService] Skip mark bad track: invalid path format" << std::endl;
                    }
                }
                if (markedBad) {
                    // 重新同步播放列表，避免坏轨继续留在内存播放队列里
                    controller_->getPlaylistManager().clear();
                    syncDatabaseTracksToPlaylist();
                }
                // 错误也交给命令线程统一切歌
                tryNext();
                break;
            }
            default:
                break;
        }
    }
}

void MusicPlayerService::eventLoop() {
    std::cout << "[MusicPlayerService] Event loop started" << std::endl;
    
    while (!should_stop_) {
        // 简单的心跳事件
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (!should_stop_) {
            json data;
            data["status"] = "alive";
            publishEvent("heartbeat", data);
        }
    }
    
    std::cout << "[MusicPlayerService] Event loop stopped" << std::endl;
}

CommandResponse MusicPlayerService::handleCommand(const CommandRequest& request) {
    try {
        if (request.command == "play") {
            return handlePlay(request.params);
        } else if (request.command == "pause") {
            return handlePause(request.params);
        } else if (request.command == "stop") {
            return handleStop(request.params);
        } else if (request.command == "next") {
            return handleNext(request.params);
        } else if (request.command == "previous") {
            return handlePrevious(request.params);
        } else if (request.command == "get_status") {
            return handleGetStatus(request.params);
        } else if (request.command == "get_track") {
            return handleGetTrack(request.params);
        } else if (request.command == "search_tracks") {
            return handleSearchTracks(request.params);
        } else if (request.command == "add_track") {
            return handleAddTrack(request.params);
        } else if (request.command == "get_all_tracks") {
            return handleGetAllTracks(request.params);
        } else if (request.command == "tag_track_emotion") {
            // 最小实现：写入 track_emotions（供 Python/服务端共用）
            // params: {track_id, mood, valence, arousal, energy, tags[]}
            if (!library_ || !library_->isOpen()) {
                return CommandResponse::error("Library not open", request.request_id);
            }
            int64_t track_id = request.params.value("track_id", -1);
            if (track_id <= 0) {
                return CommandResponse::error("track_id required", request.request_id);
            }

            std::string mood = request.params.value("mood", "");
            double valence = request.params.value("valence", 0.0);
            double arousal = request.params.value("arousal", 0.3);
            double energy = request.params.value("energy", 0.7);

            json tags = request.params.value("tags", json::array());
            std::string tags_json = tags.dump();

            if (!library_->upsertTrackEmotion(track_id, valence, arousal, energy, mood, tags_json)) {
                return CommandResponse::error("Failed to write emotion tags", request.request_id);
            }

            const auto now = static_cast<long long>(std::time(nullptr));

            json result;
            result["track_id"] = track_id;
            result["mood"] = mood;
            result["valence"] = valence;
            result["arousal"] = arousal;
            result["energy"] = energy;
            result["tags"] = tags;
            result["updated_at"] = now;
            return CommandResponse::success(result, request.request_id);
        } else if (request.command == "play_by_emotion") {
            // params: {mood?, min_valence?, max_valence?, min_arousal?, max_arousal?, min_energy?, max_energy?, tags_any?[]}
            if (!library_ || !library_->isOpen()) {
                return CommandResponse::error("Library not open", request.request_id);
            }
            // 确保播放列表存在
            if (controller_->getPlaylistSize() == 0) {
                syncDatabaseTracksToPlaylist();
            }

            // 读取筛选条件
            std::string mood = request.params.value("mood", "");
            double min_v = request.params.value("min_valence", -1.0);
            double max_v = request.params.value("max_valence", 1.0);
            double min_a = request.params.value("min_arousal", 0.0);
            double max_a = request.params.value("max_arousal", 1.0);
            double min_e = request.params.value("min_energy", 0.0);
            double max_e = request.params.value("max_energy", 1.0);
            json tags_any = request.params.value("tags_any", json::array());

            int64_t track_id = library_->pickTrackIdByEmotion(mood, min_v, max_v, min_a, max_a, min_e, max_e);
            if (track_id <= 0) {
                return CommandResponse::error("No track matched emotion filter", request.request_id);
            }

            // track_id -> playlist index
            const auto& all = controller_->getPlaylistManager().getAllTracks();
            size_t chosen = static_cast<size_t>(-1);
            for (size_t i = 0; i < all.size(); i++) {
                if (all[i].id == track_id) {
                    chosen = i;
                    break;
                }
            }
            if (chosen == static_cast<size_t>(-1)) {
                // 播放列表可能未同步到最新数据库，重新同步再找一次
                controller_->getPlaylistManager().clear();
                syncDatabaseTracksToPlaylist();
                const auto& all2 = controller_->getPlaylistManager().getAllTracks();
                for (size_t i = 0; i < all2.size(); i++) {
                    if (all2[i].id == track_id) {
                        chosen = i;
                        break;
                    }
                }
            }
            if (chosen == static_cast<size_t>(-1)) {
                return CommandResponse::error("Track matched emotion but not in playlist", request.request_id);
            }

            if (!controller_->playTrack(chosen)) {
                return CommandResponse::error("Failed to start playback", request.request_id);
            }

            json result;
            result["message"] = "Playback started by emotion";
            result["index"] = chosen;
            result["current_track"] = controller_->getCurrentTrack().title;
            result["mood"] = mood;
            return CommandResponse::success(result, request.request_id);
        } else if (request.command == "play_by_filter") {
            // params: {artist?, album?, genre?, shuffle?}
            if (!library_ || !library_->isOpen()) {
                return CommandResponse::error("Library not open", request.request_id);
            }

            std::string artist = request.params.value("artist", "");
            std::string album = request.params.value("album", "");
            std::string genre = request.params.value("genre", "");
            bool shuffle = request.params.value("shuffle", false);

            // 先确保播放列表已同步
            if (controller_->getPlaylistSize() == 0) {
                syncDatabaseTracksToPlaylist();
            }

            int64_t track_id = library_->pickRandomTrackByFilter(artist, album, genre);
            if (track_id <= 0) {
                return CommandResponse::error("No track matched filter (artist=" + artist + ", album=" + album + ", genre=" + genre + ")", request.request_id);
            }

            // 获取匹配曲目的信息
            TrackInfo track_info;
            if (!library_->getTrack(track_id, track_info)) {
                return CommandResponse::error("Failed to get track info for id=" + std::to_string(track_id), request.request_id);
            }

            // 在播放列表中按 file_path 查找（比按 id 更可靠）
            const auto& all = controller_->getPlaylistManager().getAllTracks();
            size_t chosen = static_cast<size_t>(-1);
            for (size_t i = 0; i < all.size(); i++) {
                if (all[i].file_path == track_info.file_path) {
                    chosen = i;
                    break;
                }
            }

            if (chosen == static_cast<size_t>(-1)) {
                // 播放列表可能未同步，重新同步再找一次
                controller_->getPlaylistManager().clear();
                syncDatabaseTracksToPlaylist();
                const auto& all2 = controller_->getPlaylistManager().getAllTracks();
                for (size_t i = 0; i < all2.size(); i++) {
                    if (all2[i].file_path == track_info.file_path) {
                        chosen = i;
                        break;
                    }
                }
            }

            if (chosen == static_cast<size_t>(-1)) {
                return CommandResponse::error("Track matched filter but file_path not in playlist: " + track_info.file_path, request.request_id);
            }

            if (shuffle) {
                controller_->getPlaylistManager().shuffle();
                // 重新在打乱后的列表中查找
                const auto& shuffled = controller_->getPlaylistManager().getAllTracks();
                chosen = static_cast<size_t>(-1);
                for (size_t i = 0; i < shuffled.size(); i++) {
                    if (shuffled[i].file_path == track_info.file_path) {
                        chosen = i;
                        break;
                    }
                }
                if (chosen == static_cast<size_t>(-1)) {
                    chosen = 0;  // 降级：取第一首
                }
            }

            if (!controller_->playTrack(chosen)) {
                return CommandResponse::error("Failed to start playback", request.request_id);
            }

            json result;
            result["message"] = "Playback started by filter";
            result["index"] = chosen;
            result["current_track"] = controller_->getCurrentTrack().title;
            result["artist"] = artist;
            result["album"] = album;
            result["genre"] = genre;
            result["shuffle"] = shuffle;
            return CommandResponse::success(result, request.request_id);
        } else if (request.command == "set_play_mode") {
            // params: {mode: "sequential"|"loop_all"|"random"|"single_loop"}
            std::string mode_str = request.params.value("mode", "sequential");
            PlayMode mode = PlayMode::Sequential;
            if (mode_str == "sequential") {
                mode = PlayMode::Sequential;
            } else if (mode_str == "loop_all") {
                mode = PlayMode::LoopAll;
            } else if (mode_str == "random") {
                mode = PlayMode::Random;
            } else if (mode_str == "single_loop") {
                mode = PlayMode::SingleLoop;
            } else {
                return CommandResponse::error("Unknown play mode: " + mode_str, request.request_id);
            }

            controller_->setPlayMode(mode);

            json result;
            result["message"] = "Play mode set";
            result["mode"] = mode_str;
            return CommandResponse::success(result, request.request_id);
        } else if (request.command == "set_loop_count") {
            // params: {count: int}  (0=无限制)
            int count = request.params.value("count", 0);
            if (count < 0) {
                return CommandResponse::error("Loop count must be >= 0", request.request_id);
            }

            controller_->getPlaylistManager().setLoopCount(count);

            json result;
            result["message"] = "Loop count set";
            result["count"] = count;
            result["remaining"] = controller_->getPlaylistManager().getRemainingLoops();
            return CommandResponse::success(result, request.request_id);
        } else {
            return CommandResponse::error("Unknown command: " + request.command, request.request_id);
        }
    } catch (const std::exception& e) {
        return CommandResponse::error(std::string("Command execution failed: ") + e.what(), request.request_id);
    }
}

void MusicPlayerService::publishEvent(const std::string& event_type, const json& data) {
    try {
        EventMessage event = EventMessage::create(event_type, data);
        std::string event_str = event.toJson();
        
        zmq::message_t message(event_str.size());
        memcpy(message.data(), event_str.c_str(), event_str.size());
        event_socket_->send(message, zmq::send_flags::none);
        
    } catch (const std::exception& e) {
        std::cerr << "[MusicPlayerService] Failed to publish event: " << e.what() << std::endl;
    }
}

// ========== 命令处理器 ==========

CommandResponse MusicPlayerService::handlePlay(const json& params) {
    // 如果播放列表为空，尝试从数据库加载
    if (controller_->getPlaylistSize() == 0) {
        std::cout << "[MusicPlayerService] Playlist empty, loading from database..." << std::endl;
        syncDatabaseTracksToPlaylist();
    }
    
    // 支持按索引播放
    bool success = false;
    if (params.contains("index")) {
        size_t index = params["index"];
        std::cout << "[MusicPlayerService] Playing track index: " << index << std::endl;
        success = controller_->playTrack(index);
    } else {
        success = controller_->play();
    }

    // 检查播放结果
    if (!success) {
        std::cerr << "[MusicPlayerService] ❌ Play failed" << std::endl;
        return CommandResponse::error("Failed to start playback");
    }
    
    json result;
    result["status"] = "playing";
    result["message"] = "Playback started";
    result["current_track"] = controller_->getCurrentTrack().title;
    result["position_ms"] = controller_->getPosition();
    result["duration_ms"] = controller_->getDuration();
    
    std::cout << "[MusicPlayerService] ▶️  Playing: " << result["current_track"] << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handlePause(const json& params) {
    // 真实暂停实现
    if (!controller_->pause()) {
        return CommandResponse::error("Failed to pause playback");
    }
    
    json result;
    result["status"] = "paused";
    result["message"] = "Playback paused";
    result["position_ms"] = controller_->getPosition();
    
    std::cout << "[MusicPlayerService] ⏸️  Paused at " << result["position_ms"] << "ms" << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handleStop(const json& params) {
    // 真实停止实现
    if (!controller_->stop()) {
        return CommandResponse::error("Failed to stop playback");
    }
    
    json result;
    result["status"] = "stopped";
    result["message"] = "Playback stopped";
    
    std::cout << "[MusicPlayerService] ⏹️  Stopped" << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handleNext(const json& params) {
    std::cout << "[MusicPlayerService] handleNext called" << std::endl;
    
    // 真实下一首实现
    if (!controller_->next()) {
        std::cout << "[MusicPlayerService] handleNext failed: controller->next() returned false" << std::endl;
        return CommandResponse::error("Failed to skip to next track");
    }
    
    std::cout << "[MusicPlayerService] handleNext: getting current track" << std::endl;
    Track current = controller_->getCurrentTrack();
    std::cout << "[MusicPlayerService] handleNext: got track: " << current.title << std::endl;
    
    json result;
    result["message"] = "Skipped to next track";
    result["current_track"] = current.title;
    result["position_ms"] = 0;
    
    std::cout << "[MusicPlayerService] ⏭️  Next: " << result["current_track"] << std::endl;
    std::cout << "[MusicPlayerService] handleNext returning success" << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handlePrevious(const json& params) {
    std::cout << "[MusicPlayerService] handlePrevious called" << std::endl;
    
    // 真实上一首实现
    if (!controller_->prev()) {
        std::cout << "[MusicPlayerService] handlePrevious failed: controller->prev() returned false" << std::endl;
        return CommandResponse::error("Failed to skip to previous track");
    }
    
    std::cout << "[MusicPlayerService] handlePrevious: getting current track" << std::endl;
    Track current = controller_->getCurrentTrack();
    std::cout << "[MusicPlayerService] handlePrevious: got track: " << current.title << std::endl;
    
    json result;
    result["message"] = "Skipped to previous track";
    result["current_track"] = current.title;
    result["position_ms"] = 0;
    
    std::cout << "[MusicPlayerService] ⏮️  Previous: " << result["current_track"] << std::endl;
    std::cout << "[MusicPlayerService] handlePrevious returning success" << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handleGetStatus(const json& params) {
    json result;
    
    // 获取真实播放状态
    PlayState state = controller_->getState();
    std::string state_str = "stopped";
    if (state == PlayState::Playing) {
        state_str = "playing";
    } else if (state == PlayState::Paused) {
        state_str = "paused";
    }
    
    result["state"] = state_str;
    result["current_track_id"] = controller_->getCurrentTrackIndex();
    result["position_ms"] = controller_->getPosition();
    result["duration_ms"] = controller_->getDuration();
    result["volume"] = 100;  // TODO: 从实际设备获取
    
    Track current = controller_->getCurrentTrack();
    if (!current.title.empty()) {
        result["current_track"] = current.title;
        result["artist"] = current.artist;
    }
    
    std::cout << "[MusicPlayerService] 📊 Status: " << state_str 
              << " (pos: " << result["position_ms"].get<int>() << "ms)" << std::endl;
    return CommandResponse::success(result);
}

CommandResponse MusicPlayerService::handleGetTrack(const json& params) {
    if (!params.contains("track_id")) {
        return CommandResponse::error("Missing track_id parameter");
    }
    
    int64_t track_id = params["track_id"];
    TrackInfo track;
    
    if (library_->getTrack(track_id, track)) {
        json result;
        result["id"] = track.id;
        result["title"] = track.title;
        result["artist"] = track.artist;
        result["album"] = track.album;
        result["year"] = track.year;
        result["duration_ms"] = track.duration_ms;
        result["play_count"] = track.play_count;
        result["is_favorite"] = track.is_favorite;
        return CommandResponse::success(result);
    } else {
        return CommandResponse::error("Track not found");
    }
}

CommandResponse MusicPlayerService::handleSearchTracks(const json& params) {
    std::string query = params.value("query", "");
    
    SearchCriteria criteria;
    criteria.title = query;
    criteria.limit = params.value("limit", 10);
    
    auto tracks = library_->searchTracks(criteria);
    
    json result = json::array();
    for (const auto& track : tracks) {
        json track_json;
        track_json["id"] = track.id;
        track_json["title"] = track.title;
        track_json["artist"] = track.artist;
        track_json["album"] = track.album;
        result.push_back(track_json);
    }
    
    json response;
    response["tracks"] = result;
    response["count"] = result.size();
    return CommandResponse::success(response);
}

CommandResponse MusicPlayerService::handleAddTrack(const json& params) {
    namespace fs = std::filesystem;
    
    Track track;
    std::string file_path = params.value("file_path", "");
    
    if (file_path.empty()) {
        return CommandResponse::error("Missing file_path parameter");
    }
    
    // 处理相对路径和绝对路径
    std::string abs_file_path = file_path;
    if (!fs::path(file_path).is_absolute()) {
        // 相对路径相对于工作目录
        abs_file_path = fs::absolute(file_path).string();
    }
    
    // 验证文件是否存在
    if (!fs::exists(abs_file_path)) {
        return CommandResponse::error("File not found: " + abs_file_path);
    }
    
    if (!fs::is_regular_file(abs_file_path)) {
        return CommandResponse::error("Not a regular file: " + abs_file_path);
    }
    
    track.file_path = abs_file_path;
    track.title = params.value("title", fs::path(abs_file_path).stem().string());
    track.artist = params.value("artist", "Unknown Artist");
    track.album = params.value("album", "Unknown Album");
    track.year = params.value("year", 0);
    track.duration_ms = params.value("duration_ms", 0);
    
    int64_t track_id = library_->addTrack(track);
    
    if (track_id > 0) {
        json result;
        result["track_id"] = track_id;
        result["message"] = "Track added successfully";
        result["file_path"] = abs_file_path;
        result["title"] = track.title;
        
        // 同时添加到播放列表
        controller_->getPlaylistManager().addTrack(track);
        
        // 发布事件
        json event_data;
        event_data["track_id"] = track_id;
        event_data["title"] = track.title;
        event_data["file_path"] = abs_file_path;
        publishEvent("track_added", event_data);
        
        std::cout << "[MusicPlayerService] ➕ Added track: " << track.title 
                  << " (" << abs_file_path << ")" << std::endl;
        
        return CommandResponse::success(result);
    } else {
        return CommandResponse::error("Failed to add track");
    }
}

CommandResponse MusicPlayerService::handleGetAllTracks(const json& params) {
    int limit = params.value("limit", 100);
    auto tracks = library_->getAllTracks(limit);
    
    json result = json::array();
    for (const auto& track : tracks) {
        json track_json;
        track_json["id"] = track.id;
        track_json["title"] = track.title;
        track_json["artist"] = track.artist;
        track_json["album"] = track.album;
        result.push_back(track_json);
    }
    
    json response;
    response["tracks"] = result;
    response["count"] = result.size();
    return CommandResponse::success(response);
}

// ========== 辅助方法 ==========

bool MusicPlayerService::syncDatabaseTracksToPlaylist() {
    // 从数据库获取所有曲目
    auto tracks = library_->getAllTracks(1000);  // 获取最多1000首
    
    if (tracks.empty()) {
        std::cout << "[MusicPlayerService] No tracks in database to load" << std::endl;
        return true;  // 不是错误，只是没有曲目
    }
    
    std::cout << "[MusicPlayerService] Syncing " << tracks.size() << " tracks from database to playlist..." << std::endl;
    
    // 直接添加曲目到播放列表
    for (const auto& track : tracks) {
        controller_->getPlaylistManager().addTrack(track);
        std::cout << "  ✓ Added: " << track.title << " (" << track.file_path << ")" << std::endl;
    }
    
    std::cout << "[MusicPlayerService] Playlist now has " << controller_->getPlaylistSize() 
              << " tracks" << std::endl;
    
    return true;
}

bool MusicPlayerService::scanMusicDirectories() {
    if (!library_ || !library_->isOpen()) {
        std::cerr << "[MusicPlayerService] Library not open" << std::endl;
        return false;
    }
    
    namespace fs = std::filesystem;
    int total_added = 0;
    
    // 扫描配置中的所有目录
    for (const auto& dir : config_.scan_directories) {
        std::cout << "[MusicPlayerService] Scanning directory: " << dir << std::endl;
        
        auto tracks = scanDirectory(dir);
        if (!tracks.empty()) {
            std::cout << "  Found " << tracks.size() << " tracks" << std::endl;
            int added = library_->addTracks(tracks);
            std::cout << "  Added " << added << " new tracks to database" << std::endl;
            total_added += added;
        }
    }
    
    std::cout << "[MusicPlayerService] Scan complete: " << total_added << " tracks added" << std::endl;
    return true;
}

std::vector<Track> MusicPlayerService::scanDirectory(const std::string& directory) {
    std::vector<Track> tracks;
    namespace fs = std::filesystem;
    
    // 检查目录是否存在
    if (!fs::exists(directory)) {
        std::cerr << "[MusicPlayerService] Directory not found: " << directory << std::endl;
        return tracks;
    }
    
    if (!fs::is_directory(directory)) {
        std::cerr << "[MusicPlayerService] Not a directory: " << directory << std::endl;
        return tracks;
    }
    
    // 获取绝对路径
    std::string abs_dir = fs::absolute(directory).string();
    
    // 遍历目录中的所有文件
    try {
        for (const auto& entry : fs::recursive_directory_iterator(abs_dir)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            std::string extension = entry.path().extension().string();
            
            // 转换为小写比较
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            
            // 检查是否是支持的音乐格式
            bool is_supported = false;
            for (const auto& format : config_.supported_formats) {
                if (extension == "." + format) {
                    is_supported = true;
                    break;
                }
            }
            
            if (!is_supported) continue;
            
            // 创建Track对象
            Track track;
            track.file_path = entry.path().string();
            track.title = filename.substr(0, filename.find_last_of('.'));
            track.artist = "Unknown";
            track.album = "Unknown";
            track.year = 0;
            track.duration_ms = 0;  // 实际应该用多媒体库获取
            
            tracks.push_back(track);
        }
    } catch (const std::exception& e) {
        std::cerr << "[MusicPlayerService] Error scanning directory: " << e.what() << std::endl;
    }
    
    return tracks;
}

} // namespace music_player
