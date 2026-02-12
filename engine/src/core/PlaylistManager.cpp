// PlaylistManager.cpp
// 播放列表管理器实现

#include "PlaylistManager.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace fs = std::filesystem;

namespace music_player {

PlaylistManager::PlaylistManager()
    : currentIndex_(0)
    , playMode_(PlayMode::Sequential)
    , isShuffled_(false)
{
    // 使用当前时间作为随机种子
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    randomEngine_.seed(static_cast<unsigned int>(seed));
}

bool PlaylistManager::loadFromDirectory(const std::string& dirPath) {
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "[PlaylistManager] Directory not found: " << dirPath << std::endl;
        return false;
    }
    
    clear();
    
    try {
        // 遍历目录查找音频文件
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && isAudioFile(entry.path().string())) {
                Track track = createTrackFromFile(entry.path().string());
                tracks_.push_back(track);
            }
        }
        
        if (tracks_.empty()) {
            std::cerr << "[PlaylistManager] No audio files found in: " << dirPath << std::endl;
            return false;
        }
        
        // 按文件名排序
        std::sort(tracks_.begin(), tracks_.end(), 
                  [](const Track& a, const Track& b) {
                      return a.file_path < b.file_path;
                  });
        
        // 保存原始顺序
        originalTracks_ = tracks_;
        currentIndex_ = 0;
        
        std::cout << "[PlaylistManager] Loaded " << tracks_.size() 
                  << " tracks from: " << dirPath << std::endl;
        return true;
        
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[PlaylistManager] Filesystem error: " << e.what() << std::endl;
        return false;
    }
}

bool PlaylistManager::loadFromFile(const std::string& filePath) {
    if (!fs::exists(filePath) || !isAudioFile(filePath)) {
        std::cerr << "[PlaylistManager] Invalid audio file: " << filePath << std::endl;
        return false;
    }
    
    clear();
    
    Track track = createTrackFromFile(filePath);
    tracks_.push_back(track);
    originalTracks_ = tracks_;
    currentIndex_ = 0;
    
    std::cout << "[PlaylistManager] Loaded single track: " << filePath << std::endl;
    return true;
}

void PlaylistManager::addTrack(const Track& track) {
    tracks_.push_back(track);
    if (!isShuffled_) {
        originalTracks_ = tracks_;
    }
}

void PlaylistManager::clear() {
    tracks_.clear();
    originalTracks_.clear();
    currentIndex_ = 0;
    isShuffled_ = false;
}

void PlaylistManager::setPlayMode(PlayMode mode) {
    playMode_ = mode;
    std::cout << "[PlaylistManager] Play mode set to: " 
              << static_cast<int>(mode) << std::endl;
}

bool PlaylistManager::next() {
    if (tracks_.empty()) {
        return false;
    }
    
    size_t nextIdx = calculateNextIndex();
    
    // Sequential模式下，如果到达末尾则返回false
    if (playMode_ == PlayMode::Sequential && 
        currentIndex_ == tracks_.size() - 1) {
        return false;
    }
    
    currentIndex_ = nextIdx;
    return true;
}

bool PlaylistManager::prev() {
    if (tracks_.empty()) {
        return false;
    }
    
    size_t prevIdx = calculatePrevIndex();
    
    // Sequential模式下，如果在开头则返回false
    if (playMode_ == PlayMode::Sequential && currentIndex_ == 0) {
        return false;
    }
    
    currentIndex_ = prevIdx;
    return true;
}

bool PlaylistManager::seekTo(size_t index) {
    if (index >= tracks_.size()) {
        return false;
    }
    currentIndex_ = index;
    return true;
}

void PlaylistManager::shuffle() {
    if (tracks_.empty() || isShuffled_) {
        return;
    }
    
    // 保存当前曲目
    Track currentTrack = tracks_[currentIndex_];
    
    // 打乱列表
    std::shuffle(tracks_.begin(), tracks_.end(), randomEngine_);
    
    // 找到当前曲目的新位置
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                          [&currentTrack](const Track& t) {
                              return t.file_path == currentTrack.file_path;
                          });
    
    if (it != tracks_.end()) {
        currentIndex_ = std::distance(tracks_.begin(), it);
    } else {
        currentIndex_ = 0;
    }
    
    isShuffled_ = true;
    std::cout << "[PlaylistManager] Playlist shuffled" << std::endl;
}

void PlaylistManager::unshuffle() {
    if (!isShuffled_) {
        return;
    }
    
    // 保存当前曲目
    Track currentTrack = tracks_[currentIndex_];
    
    // 恢复原始顺序
    tracks_ = originalTracks_;
    
    // 找到当前曲目的位置
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                          [&currentTrack](const Track& t) {
                              return t.file_path == currentTrack.file_path;
                          });
    
    if (it != tracks_.end()) {
        currentIndex_ = std::distance(tracks_.begin(), it);
    } else {
        currentIndex_ = 0;
    }
    
    isShuffled_ = false;
    std::cout << "[PlaylistManager] Playlist restored to original order" << std::endl;
}

Track PlaylistManager::getCurrentTrack() const {
    if (tracks_.empty()) {
        return Track{};  // 返回空Track
    }
    return tracks_[currentIndex_];
}

bool PlaylistManager::hasNext() const {
    if (tracks_.empty()) {
        return false;
    }
    
    switch (playMode_) {
        case PlayMode::Sequential:
            return currentIndex_ < tracks_.size() - 1;
        case PlayMode::LoopAll:
        case PlayMode::Random:
        case PlayMode::SingleLoop:
            return true;  // 循环模式总是有下一首
    }
    return false;
}

bool PlaylistManager::hasPrev() const {
    if (tracks_.empty()) {
        return false;
    }
    
    switch (playMode_) {
        case PlayMode::Sequential:
            return currentIndex_ > 0;
        case PlayMode::LoopAll:
        case PlayMode::Random:
        case PlayMode::SingleLoop:
            return true;  // 循环模式总是有上一首
    }
    return false;
}

size_t PlaylistManager::calculateNextIndex() const {
    if (tracks_.empty()) {
        return 0;
    }
    
    switch (playMode_) {
        case PlayMode::Sequential:
            // 顺序播放：到末尾就停止
            return std::min(currentIndex_ + 1, tracks_.size() - 1);
            
        case PlayMode::LoopAll:
            // 列表循环：到末尾回到开头
            return (currentIndex_ + 1) % tracks_.size();
            
        case PlayMode::Random:
            // 随机播放：随机选择（但不重复当前）
            if (tracks_.size() == 1) {
                return 0;
            }
            size_t nextIdx;
            do {
                nextIdx = randomEngine_() % tracks_.size();
            } while (nextIdx == currentIndex_);
            return nextIdx;
            
        case PlayMode::SingleLoop:
            // 单曲循环：保持当前索引
            return currentIndex_;
    }
    
    return currentIndex_;
}

size_t PlaylistManager::calculatePrevIndex() const {
    if (tracks_.empty()) {
        return 0;
    }
    
    switch (playMode_) {
        case PlayMode::Sequential:
            // 顺序播放：到开头就停止
            return currentIndex_ > 0 ? currentIndex_ - 1 : 0;
            
        case PlayMode::LoopAll:
            // 列表循环：到开头回到末尾
            return currentIndex_ > 0 ? currentIndex_ - 1 : tracks_.size() - 1;
            
        case PlayMode::Random:
            // 随机播放：随机选择（但不重复当前）
            if (tracks_.size() == 1) {
                return 0;
            }
            size_t prevIdx;
            do {
                prevIdx = randomEngine_() % tracks_.size();
            } while (prevIdx == currentIndex_);
            return prevIdx;
            
        case PlayMode::SingleLoop:
            // 单曲循环：保持当前索引
            return currentIndex_;
    }
    
    return currentIndex_;
}

bool PlaylistManager::isAudioFile(const std::string& filename) const {
    // 支持的音频格式
    static const std::vector<std::string> extensions = {
        ".mp3", ".MP3",
        ".wav", ".WAV",
        ".m4a", ".M4A",
        ".aac", ".AAC",
        ".flac", ".FLAC"
    };
    
    for (const auto& ext : extensions) {
        if (filename.size() >= ext.size() &&
            filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

Track PlaylistManager::createTrackFromFile(const std::string& filePath) const {
    Track track;
    track.file_path = filePath;
    
    // 从文件名提取标题（去除路径和扩展名）
    fs::path path(filePath);
    track.title = path.stem().string();
    
    // TODO: 后续可以添加ID3标签解析来获取真实的元数据
    track.artist = "Unknown Artist";
    track.album = "Unknown Album";
    track.year = 0;
    track.duration_ms = 0;  // 需要播放时才能获取
    
    return track;
}

} // namespace music_player
