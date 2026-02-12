#ifndef LITE_PLAYER_WRAPPER_H
#define LITE_PLAYER_WRAPPER_H

#include "MusicPlayerTypes.h"
#include <functional>
#include <memory>

extern "C" {
#include "liteplayer_listplayer.h"
}

namespace music_player {

// 状态回调函数类型
using StateCallback = std::function<void(PlayState state, int error_code)>;

/**
 * @brief LitePlayer C API 的 C++ 包装器
 * 
 * 提供RAII风格的资源管理和C++友好的接口
 */
class LitePlayerWrapper {
public:
    LitePlayerWrapper();
    ~LitePlayerWrapper();
    
    // 禁止拷贝
    LitePlayerWrapper(const LitePlayerWrapper&) = delete;
    LitePlayerWrapper& operator=(const LitePlayerWrapper&) = delete;
    
    /**
     * @brief 初始化播放器
     * @return true 成功，false 失败
     */
    bool initialize();
    
    /**
     * @brief 设置状态回调
     */
    void setStateCallback(StateCallback callback);
    
    /**
     * @brief 加载播放列表
     * @param playlist_file 播放列表文件路径
     * @return true 成功，false 失败
     */
    bool loadPlaylist(const std::string& playlist_file);
    
    /**
     * @brief 加载单个文件
     * @param file_path 文件路径
     * @return true 成功，false 失败
     */
    bool loadFile(const std::string& file_path);
    
    /**
     * @brief 开始播放
     */
    bool start();
    
    /**
     * @brief 暂停播放
     */
    bool pause();
    
    /**
     * @brief 继续播放
     */
    bool resume();
    
    /**
     * @brief 停止播放
     */
    bool stop();
    
    /**
     * @brief 重置播放器
     */
    bool reset();
    
    /**
     * @brief 切换到下一首
     */
    bool next();
    
    /**
     * @brief 切换到上一首
     */
    bool prev();
    
    /**
     * @brief 跳转到指定位置
     * @param position_ms 位置（毫秒）
     */
    bool seek(int position_ms);
    
    /**
     * @brief 设置单曲循环
     */
    bool setSingleLooping(bool enable);
    
    /**
     * @brief 获取当前播放位置
     * @return 位置（毫秒），-1表示失败
     */
    int getPosition() const;
    
    /**
     * @brief 获取当前曲目时长
     * @return 时长（毫秒），-1表示失败
     */
    int getDuration() const;
    
    /**
     * @brief 获取当前状态
     */
    PlayState getState() const { return current_state_; }
    
private:
    // liteplayer状态回调（C风格）
    static int stateCallbackC(enum liteplayer_state state, int errcode, void* priv);
    
    // 转换liteplayer状态到PlayState
    static PlayState convertState(enum liteplayer_state state);
    
    listplayer_handle_t player_handle_;
    StateCallback state_callback_;
    PlayState current_state_;
    bool initialized_;
};

} // namespace music_player

#endif // LITE_PLAYER_WRAPPER_H
