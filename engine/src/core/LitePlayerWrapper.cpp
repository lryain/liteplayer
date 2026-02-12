#include "LitePlayerWrapper.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

extern "C" {
#include "source_file_wrapper.h"
#include "sink_alsa_wrapper.h"
}

namespace music_player {

LitePlayerWrapper::LitePlayerWrapper()
    : player_handle_(nullptr)
    , current_state_(PlayState::Idle)
    , initialized_(false) {
}

LitePlayerWrapper::~LitePlayerWrapper() {
    if (player_handle_) {
        listplayer_stop(player_handle_);
        listplayer_reset(player_handle_);
        listplayer_destroy(player_handle_);
        player_handle_ = nullptr;
    }
}

bool LitePlayerWrapper::initialize() {
    if (initialized_) {
        return true;
    }
    
    // 创建播放器配置
    struct listplayer_cfg cfg = DEFAULT_LISTPLAYER_CFG();
    
    // 创建播放器实例
    player_handle_ = listplayer_create(&cfg);
    if (!player_handle_) {
        std::cerr << "Failed to create listplayer" << std::endl;
        return false;
    }
    
    // 注册状态监听器
    listplayer_register_state_listener(player_handle_, stateCallbackC, this);
    
    // 注册音频输出适配器（ALSA）
    struct sink_wrapper sink_ops = {
        .priv_data = nullptr,
        .name = alsa_wrapper_name,
        .open = alsa_wrapper_open,
        .write = alsa_wrapper_write,
        .close = alsa_wrapper_close,
    };
    listplayer_register_sink_wrapper(player_handle_, &sink_ops);
    
    // 注册文件输入适配器
    struct source_wrapper file_ops = {
        .async_mode = false,
        .buffer_size = 2*1024,
        .priv_data = nullptr,
        .url_protocol = file_wrapper_url_protocol,
        .open = file_wrapper_open,
        .read = file_wrapper_read,
        .content_pos = file_wrapper_content_pos,
        .content_len = file_wrapper_content_len,
        .seek = file_wrapper_seek,
        .close = file_wrapper_close,
    };
    listplayer_register_source_wrapper(player_handle_, &file_ops);
    
    initialized_ = true;
    std::cout << "[LitePlayerWrapper] Initialized successfully" << std::endl;
    return true;
}

void LitePlayerWrapper::setStateCallback(StateCallback callback) {
    state_callback_ = callback;
}

bool LitePlayerWrapper::loadPlaylist(const std::string& playlist_file) {
    if (!initialized_ || !player_handle_) {
        std::cerr << "Player not initialized" << std::endl;
        return false;
    }
    
    std::cout << "[LitePlayerWrapper] loadPlaylist calling listplayer_set_data_source: " << playlist_file << std::endl;
    if (listplayer_set_data_source(player_handle_, playlist_file.c_str()) != 0) {
        std::cerr << "Failed to set data source: " << playlist_file << std::endl;
        return false;
    }
    
    std::cout << "[LitePlayerWrapper] loadPlaylist calling listplayer_prepare_async" << std::endl;
    if (listplayer_prepare_async(player_handle_) != 0) {
        std::cerr << "Failed to prepare player" << std::endl;
        return false;
    }
    std::cout << "[LitePlayerWrapper] loadPlaylist finished" << std::endl;
    
    return true;
}

bool LitePlayerWrapper::loadFile(const std::string& file_path) {
    std::cout << "[LitePlayerWrapper] loadFile called: " << file_path << std::endl;
    
    // 对于单个文件，直接使用 loadPlaylist
    bool result = loadPlaylist(file_path);
    
    std::cout << "[LitePlayerWrapper] loadFile result: " << (result ? "success" : "failed") << std::endl;
    
    return result;
}

bool LitePlayerWrapper::start() {
    if (!player_handle_) {
        std::cerr << "[LitePlayerWrapper] start: player not initialized" << std::endl;
        return false;
    }
    
    std::cout << "[LitePlayerWrapper] start called, current state=" << static_cast<int>(current_state_) << std::endl;
    
    int result = listplayer_start(player_handle_);
    
    std::cout << "[LitePlayerWrapper] start result=" << result << std::endl;
    
    return result == 0;
}

bool LitePlayerWrapper::pause() {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_pause(player_handle_) == 0;
}

bool LitePlayerWrapper::resume() {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_resume(player_handle_) == 0;
}

bool LitePlayerWrapper::stop() {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_stop(player_handle_) == 0;
}

bool LitePlayerWrapper::reset() {
    if (!player_handle_) {
        return false;
    }
    std::cout << "[LitePlayerWrapper] reset called" << std::endl;
    int res = listplayer_reset(player_handle_);
    std::cout << "[LitePlayerWrapper] reset result=" << res << std::endl;
    return res == 0;
}

bool LitePlayerWrapper::next() {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_switch_next(player_handle_) == 0;
}

bool LitePlayerWrapper::prev() {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_switch_prev(player_handle_) == 0;
}

bool LitePlayerWrapper::seek(int position_ms) {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_seek(player_handle_, position_ms) == 0;
}

bool LitePlayerWrapper::setSingleLooping(bool enable) {
    if (!player_handle_) {
        return false;
    }
    
    return listplayer_set_single_looping(player_handle_, enable) == 0;
}

int LitePlayerWrapper::getPosition() const {
    if (!player_handle_) {
        return -1;
    }
    
    int position = 0;
    if (listplayer_get_position(player_handle_, &position) != 0) {
        return -1;
    }
    
    return position;
}

int LitePlayerWrapper::getDuration() const {
    if (!player_handle_) {
        return -1;
    }
    
    int duration = 0;
    if (listplayer_get_duration(player_handle_, &duration) != 0) {
        return -1;
    }
    
    return duration;
}

// 静态回调函数
int LitePlayerWrapper::stateCallbackC(enum liteplayer_state state, int errcode, void* priv) {
    LitePlayerWrapper* wrapper = static_cast<LitePlayerWrapper*>(priv);
    if (!wrapper) {
        return -1;
    }
    
    PlayState new_state = convertState(state);
    wrapper->current_state_ = new_state;
    
    // 调用C++回调
    if (wrapper->state_callback_) {
        wrapper->state_callback_(new_state, errcode);
    }
    
    return 0;
}

PlayState LitePlayerWrapper::convertState(enum liteplayer_state state) {
    switch (state) {
        case LITEPLAYER_IDLE:
            return PlayState::Idle;
        case LITEPLAYER_INITED:
        case LITEPLAYER_PREPARED:
            return PlayState::Loading;
        case LITEPLAYER_STARTED:
            return PlayState::Playing;
        case LITEPLAYER_PAUSED:
            return PlayState::Paused;
        case LITEPLAYER_STOPPED:
        case LITEPLAYER_COMPLETED:
            return PlayState::Stopped;
        case LITEPLAYER_ERROR:
            return PlayState::Error;
        default:
            return PlayState::Idle;
    }
}

} // namespace music_player
