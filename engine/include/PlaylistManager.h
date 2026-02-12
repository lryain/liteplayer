// PlaylistManager.h
// 播放列表管理器 - 负责曲目列表管理和播放模式逻辑

#pragma once

#include "MusicPlayerTypes.h"
#include <vector>
#include <string>
#include <random>
#include <algorithm>

namespace music_player {

class PlaylistManager {
public:
    PlaylistManager();
    ~PlaylistManager() = default;

    // 播放列表管理
    bool loadFromDirectory(const std::string& dirPath);
    bool loadFromFile(const std::string& filePath);
    void addTrack(const Track& track);
    void clear();
    
    // 播放模式
    void setPlayMode(PlayMode mode);
    PlayMode getPlayMode() const { return playMode_; }
    
    // 曲目导航
    bool next();  // 移动到下一首，返回是否成功
    bool prev();  // 移动到上一首，返回是否成功
    bool seekTo(size_t index);  // 跳转到指定曲目
    
    // 随机播放
    void shuffle();  // 打乱播放列表
    void unshuffle();  // 恢复原始顺序
    
    // 查询
    Track getCurrentTrack() const;
    size_t getCurrentIndex() const { return currentIndex_; }
    size_t getTrackCount() const { return tracks_.size(); }
    bool isEmpty() const { return tracks_.empty(); }
    bool hasNext() const;
    bool hasPrev() const;
    
    // 获取整个列表（调试用）
    const std::vector<Track>& getAllTracks() const { return tracks_; }

private:
    // 计算下一个/上一个索引（根据播放模式）
    size_t calculateNextIndex() const;
    size_t calculatePrevIndex() const;
    
    // 文件系统操作
    bool isAudioFile(const std::string& filename) const;
    Track createTrackFromFile(const std::string& filePath) const;
    
    std::vector<Track> tracks_;           // 当前播放列表
    std::vector<Track> originalTracks_;   // 原始顺序（用于unshuffle）
    size_t currentIndex_;                 // 当前播放的曲目索引
    PlayMode playMode_;                   // 播放模式
    
    mutable std::mt19937 randomEngine_;   // 随机数生成器（mutable因为在const方法中使用）
    bool isShuffled_;                     // 是否处于随机状态
};

} // namespace music_player
