#ifndef MUSIC_PLAYER_TYPES_H
#define MUSIC_PLAYER_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

namespace music_player {

// 播放状态
enum class PlayState {
    Idle,           // 空闲
    Loading,        // 加载中
    Playing,        // 播放中
    Paused,         // 已暂停
    Stopped,        // 已停止
    Error           // 错误
};

// 播放模式
enum class PlayMode {
    Sequential,     // 顺序播放
    LoopAll,        // 列表循环
    Random,         // 随机播放
    SingleLoop      // 单曲循环
};

// 曲目信息
struct Track {
    int64_t id;
    std::string file_path;
    std::string file_name;
    
    // 基本信息
    std::string title;
    std::string artist;
    std::string album;
    int year;
    int duration_ms;
    
    Track() : id(-1), year(0), duration_ms(0) {}
};

} // namespace music_player

#endif // MUSIC_PLAYER_TYPES_H
