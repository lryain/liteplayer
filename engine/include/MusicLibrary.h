// MusicLibrary.h
// 音乐库管理 - SQLite3数据库封装

#pragma once

#include "MusicPlayerTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <ctime>

// 前向声明
struct sqlite3;

namespace music_player {

// 专辑信息
struct Album {
    int64_t id = 0;
    std::string name;
    std::string artist;
    int year = 0;
    int track_count = 0;
};

// 艺术家信息
struct Artist {
    int64_t id = 0;
    std::string name;
    int album_count = 0;
    int track_count = 0;
};

// 扩展的Track信息（包含数据库ID和统计信息）
struct TrackInfo : public Track {
    int64_t id = 0;
    int64_t album_id = 0;
    int64_t artist_id = 0;
    int play_count = 0;
    time_t last_played = 0;
    time_t added_date = 0;
    bool is_favorite = false;
};

// 播放列表信息
struct PlaylistInfo {
    int64_t id = 0;
    std::string name;
    std::string description;
    int track_count = 0;
    time_t created_date = 0;
    time_t modified_date = 0;
};

// 搜索条件
struct SearchCriteria {
    std::string title;
    std::string artist;
    std::string album;
    int min_year = 0;
    int max_year = 9999;
    bool favorites_only = false;
    std::string order_by = "title";  // 排序字段
    bool descending = false;          // 是否降序
    int limit = 0;                    // 限制数量（0表示无限制）
};

/**
 * @brief 音乐库管理类
 * 
 * 提供音乐库的CRUD操作、搜索、统计等功能
 */
class MusicLibrary {
public:
    MusicLibrary();
    ~MusicLibrary();

    // 禁止拷贝
    MusicLibrary(const MusicLibrary&) = delete;
    MusicLibrary& operator=(const MusicLibrary&) = delete;

    /**
     * @brief 打开或创建数据库
     * @param db_path 数据库文件路径
     * @return true 成功，false 失败
     */
    bool open(const std::string& db_path);

    /**
     * @brief 关闭数据库
     */
    void close();

    /**
     * @brief 数据库是否已打开
     */
    bool isOpen() const;

    // ========== 坏轨隔离（兜底） ==========
    /**
     * @brief 标记某个文件为“坏轨”，后续将默认从播放/列表查询中排除
     */
    bool markTrackBadByPath(const std::string& file_path, const std::string& reason);

    /**
     * @brief 取消坏轨标记
     */
    bool unmarkTrackBadByPath(const std::string& file_path);

    /**
     * @brief 判断是否坏轨
     */
    bool isTrackBadByPath(const std::string& file_path);

    // ========== 曲目管理 ==========

    /**
     * @brief 添加曲目到数据库
     * @param track 曲目信息
     * @return 曲目ID，失败返回-1
     */
    int64_t addTrack(const Track& track);

    /**
     * @brief 批量添加曲目
     * @param tracks 曲目列表
     * @return 成功添加的数量
     */
    int addTracks(const std::vector<Track>& tracks);

    /**
     * @brief 更新曲目信息
     * @param id 曲目ID
     * @param track 新的曲目信息
     * @return true 成功，false 失败
     */
    bool updateTrack(int64_t id, const Track& track);

    /**
     * @brief 删除曲目
     * @param id 曲目ID
     * @return true 成功，false 失败
     */
    bool deleteTrack(int64_t id);

    /**
     * @brief 根据ID获取曲目
     * @param id 曲目ID
     * @param track 输出参数
     * @return true 成功，false 失败
     */
    bool getTrack(int64_t id, TrackInfo& track);

    /**
     * @brief 获取所有曲目
     * @param limit 限制数量（0表示无限制）
     * @return 曲目列表
     */
    std::vector<TrackInfo> getAllTracks(int limit = 0);

    // ========== 搜索 ==========

    /**
     * @brief 搜索曲目
     * @param criteria 搜索条件
     * @return 匹配的曲目列表
     */
    std::vector<TrackInfo> searchTracks(const SearchCriteria& criteria);

    /**
     * @brief 按艺术家搜索
     */
    std::vector<TrackInfo> getTracksByArtist(const std::string& artist, int limit = 0);

    /**
     * @brief 按专辑搜索
     */
    std::vector<TrackInfo> getTracksByAlbum(const std::string& album, int limit = 0);

    // ========== 统计和历史 ==========

    /**
     * @brief 记录播放
     * @param track_id 曲目ID
     */
    void recordPlay(int64_t track_id);

    /**
     * @brief 设置收藏状态
     */
    bool setFavorite(int64_t track_id, bool is_favorite);

    /**
     * @brief 获取收藏列表
     */
    std::vector<TrackInfo> getFavorites();

    /**
     * @brief 获取最近播放
     * @param limit 限制数量
     */
    std::vector<TrackInfo> getRecentlyPlayed(int limit = 20);

    /**
     * @brief 获取播放次数最多的曲目
     */
    std::vector<TrackInfo> getMostPlayed(int limit = 20);

    // ========== 专辑管理 ==========

    /**
     * @brief 获取所有专辑
     */
    std::vector<Album> getAllAlbums();

    /**
     * @brief 获取专辑信息
     */
    bool getAlbum(int64_t id, Album& album);

    // ========== 艺术家管理 ==========

    /**
     * @brief 获取所有艺术家
     */
    std::vector<Artist> getAllArtists();

    /**
     * @brief 获取艺术家信息
     */
    bool getArtist(int64_t id, Artist& artist);

    // ========== 播放列表 ==========

    /**
     * @brief 创建播放列表
     */
    int64_t createPlaylist(const std::string& name, const std::string& description = "");

    /**
     * @brief 删除播放列表
     */
    bool deletePlaylist(int64_t id);

    /**
     * @brief 添加曲目到播放列表
     */
    bool addTrackToPlaylist(int64_t playlist_id, int64_t track_id, int position = -1);

    /**
     * @brief 从播放列表删除曲目
     */
    bool removeTrackFromPlaylist(int64_t playlist_id, int64_t track_id);

    /**
     * @brief 获取播放列表中的曲目
     */
    std::vector<TrackInfo> getPlaylistTracks(int64_t playlist_id);

    /**
     * @brief 获取所有播放列表
     */
    std::vector<PlaylistInfo> getAllPlaylists();

    // ========== 统计信息 ==========

    /**
     * @brief 获取数据库统计信息
     */
    struct Stats {
        int total_tracks = 0;
        int total_albums = 0;
        int total_artists = 0;
        int total_playlists = 0;
        int64_t total_duration_ms = 0;
    };
    Stats getStats();

private:
    // 初始化数据库表
    bool initializeTables();

    // 获取或创建艺术家ID
    int64_t getOrCreateArtist(const std::string& artist_name);

    // 获取或创建专辑ID
    int64_t getOrCreateAlbum(const std::string& album_name, const std::string& artist_name, int year);

    // 执行SQL语句
    bool execute(const std::string& sql);

    sqlite3* db_;
    bool is_open_;
};

} // namespace music_player
