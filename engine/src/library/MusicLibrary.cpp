// MusicLibrary.cpp
// 音乐库管理实现

#include "MusicLibrary.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <ctime>

namespace music_player {

static bool columnExists(sqlite3* db, const std::string& table, const std::string& column) {
    std::string sql = "PRAGMA table_info(" + table + ")";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        if (name && column == reinterpret_cast<const char*>(name)) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

MusicLibrary::MusicLibrary()
    : db_(nullptr)
    , is_open_(false)
{
}

MusicLibrary::~MusicLibrary() {
    close();
}

bool MusicLibrary::open(const std::string& db_path) {
    if (is_open_) {
        std::cerr << "[MusicLibrary] Database already open" << std::endl;
        return false;
    }
    
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicLibrary] Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    is_open_ = true;
    
    // 初始化表结构
    if (!initializeTables()) {
        close();
        return false;
    }

    // 兜底迁移：坏轨隔离字段
    // 注意：SQLite 的 ALTER TABLE ADD COLUMN 是幂等的（对已存在列会报错），所以这里先探测列存在性。
    if (db_) {
        try {
            if (!columnExists(db_, "tracks", "bad_flag")) {
                execute("ALTER TABLE tracks ADD COLUMN bad_flag INTEGER DEFAULT 0;");
            }
            if (!columnExists(db_, "tracks", "bad_reason")) {
                execute("ALTER TABLE tracks ADD COLUMN bad_reason TEXT;");
            }
            if (!columnExists(db_, "tracks", "bad_marked_at")) {
                execute("ALTER TABLE tracks ADD COLUMN bad_marked_at INTEGER DEFAULT 0;");
            }
            if (!columnExists(db_, "tracks", "bad_fail_count")) {
                execute("ALTER TABLE tracks ADD COLUMN bad_fail_count INTEGER DEFAULT 0;");
            }
            execute("CREATE INDEX IF NOT EXISTS idx_tracks_bad_flag ON tracks(bad_flag);");
        } catch (...) {
            // 不影响服务启动
        }
    }
    
    std::cout << "[MusicLibrary] Database opened: " << db_path << std::endl;
    return true;
}

bool MusicLibrary::markTrackBadByPath(const std::string& file_path, const std::string& reason) {
    if (!is_open_ || file_path.empty()) return false;

    const char* sql = R"(
        UPDATE tracks
        SET bad_flag = 1,
            bad_reason = ?,
            bad_marked_at = ?,
            bad_fail_count = bad_fail_count + 1
        WHERE file_path = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    const auto now = static_cast<sqlite3_int64>(std::time(nullptr));
    sqlite3_bind_text(stmt, 1, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, file_path.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (ok) {
        std::cout << "[MusicLibrary] Marked bad track: " << file_path << " reason=" << reason << std::endl;
    }
    return ok;
}

bool MusicLibrary::unmarkTrackBadByPath(const std::string& file_path) {
    if (!is_open_ || file_path.empty()) return false;

    const char* sql = R"(
        UPDATE tracks
        SET bad_flag = 0,
            bad_reason = NULL,
            bad_marked_at = 0
        WHERE file_path = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool MusicLibrary::isTrackBadByPath(const std::string& file_path) {
    if (!is_open_ || file_path.empty()) return false;

    const char* sql = "SELECT bad_flag FROM tracks WHERE file_path = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);

    bool bad = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        bad = (sqlite3_column_int(stmt, 0) != 0);
    }
    sqlite3_finalize(stmt);
    return bad;
}

bool MusicLibrary::updateTrack(int64_t id, const Track& track) {
    if (!is_open_) return false;

    // 获取或创建艺术家和专辑
    int64_t artist_id = getOrCreateArtist(track.artist);
    int64_t album_id = getOrCreateAlbum(track.album, track.artist, track.year);

    const char* sql = R"(
        UPDATE tracks 
        SET file_path = ?, title = ?, artist = ?, album = ?, 
            year = ?, duration_ms = ?, artist_id = ?, album_id = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, track.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, track.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, track.year);
    sqlite3_bind_int64(stmt, 6, track.duration_ms);
    sqlite3_bind_int64(stmt, 7, artist_id);
    sqlite3_bind_int64(stmt, 8, album_id);
    sqlite3_bind_int64(stmt, 9, id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (success) {
        std::cout << "[MusicLibrary] Updated track ID: " << id << std::endl;
    }
    
    return success;
}

bool MusicLibrary::deleteTrack(int64_t id) {
    if (!is_open_) return false;

    // 先从播放列表中删除该曲目
    const char* delete_from_playlists = "DELETE FROM playlist_tracks WHERE track_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, delete_from_playlists, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 删除曲目
    const char* delete_track = "DELETE FROM tracks WHERE id = ?";
    if (sqlite3_prepare_v2(db_, delete_track, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (success) {
        std::cout << "[MusicLibrary] Deleted track ID: " << id << std::endl;
    }
    
    return success;
}

void MusicLibrary::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    is_open_ = false;
}

bool MusicLibrary::isOpen() const {
    return is_open_;
}

bool MusicLibrary::initializeTables() {
    // 创建艺术家表
    const char* create_artists = R"(
        CREATE TABLE IF NOT EXISTS artists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            album_count INTEGER DEFAULT 0,
            track_count INTEGER DEFAULT 0
        )
    )";

    // 创建专辑表
    const char* create_albums = R"(
        CREATE TABLE IF NOT EXISTS albums (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            artist TEXT,
            year INTEGER DEFAULT 0,
            track_count INTEGER DEFAULT 0
        )
    )";

    // 创建曲目表
    const char* create_tracks = R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL UNIQUE,
            title TEXT NOT NULL,
            artist TEXT,
            album TEXT,
            year INTEGER DEFAULT 0,
            duration_ms INTEGER DEFAULT 0,
            artist_id INTEGER,
            album_id INTEGER,
            play_count INTEGER DEFAULT 0,
            last_played INTEGER DEFAULT 0,
            added_date INTEGER DEFAULT 0,
            is_favorite INTEGER DEFAULT 0,
            FOREIGN KEY (artist_id) REFERENCES artists(id),
            FOREIGN KEY (album_id) REFERENCES albums(id)
        )
    )";

    // 创建播放列表表
    const char* create_playlists = R"(
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT,
            created_date INTEGER DEFAULT 0,
            modified_date INTEGER DEFAULT 0,
            track_count INTEGER DEFAULT 0
        )
    )";

    // 创建播放列表曲目关联表
    const char* create_playlist_tracks = R"(
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            playlist_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            position INTEGER NOT NULL,
            added_date INTEGER DEFAULT 0,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
            FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE,
            PRIMARY KEY (playlist_id, track_id)
        )
    )";

    // 创建索引
    const char* create_indexes = R"(
        CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist);
        CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album);
        CREATE INDEX IF NOT EXISTS idx_tracks_title ON tracks(title);
        CREATE INDEX IF NOT EXISTS idx_tracks_play_count ON tracks(play_count DESC);
        CREATE INDEX IF NOT EXISTS idx_tracks_last_played ON tracks(last_played DESC);
    )";

    return execute(create_artists) &&
           execute(create_albums) &&
           execute(create_tracks) &&
           execute(create_playlists) &&
           execute(create_playlist_tracks) &&
           execute(create_indexes);
}

bool MusicLibrary::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicLibrary] SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

int64_t MusicLibrary::addTrack(const Track& track) {
    if (!is_open_) {
        std::cerr << "[MusicLibrary] Database not open" << std::endl;
        return -1;
    }

    // 获取或创建艺术家和专辑
    int64_t artist_id = getOrCreateArtist(track.artist);
    int64_t album_id = getOrCreateAlbum(track.album, track.artist, track.year);

    const char* sql = R"(
        INSERT OR IGNORE INTO tracks (file_path, title, artist, album, year, duration_ms, 
                           artist_id, album_id, added_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicLibrary] Prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    time_t now = std::time(nullptr);
    sqlite3_bind_text(stmt, 1, track.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, track.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, track.year);
    sqlite3_bind_int64(stmt, 6, track.duration_ms);
    sqlite3_bind_int64(stmt, 7, artist_id);
    sqlite3_bind_int64(stmt, 8, album_id);
    sqlite3_bind_int64(stmt, 9, now);

    rc = sqlite3_step(stmt);
    int64_t track_id = -1;
    
    if (rc == SQLITE_DONE) {
        track_id = sqlite3_last_insert_rowid(db_);
        if (track_id > 0) {
            std::cout << "[MusicLibrary] Added track: " << track.title 
                      << " (ID: " << track_id << ")" << std::endl;
        } else {
            // 文件已存在，跳过
            std::cout << "[MusicLibrary] Track already exists: " << track.file_path << std::endl;
        }
    } else {
        std::cerr << "[MusicLibrary] Insert failed: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return track_id;
}

int MusicLibrary::addTracks(const std::vector<Track>& tracks) {
    int count = 0;
    
    // 使用事务提高性能
    execute("BEGIN TRANSACTION");
    
    for (const auto& track : tracks) {
        if (addTrack(track) >= 0) {
            count++;
        }
    }
    
    execute("COMMIT");
    
    std::cout << "[MusicLibrary] Added " << count << "/" << tracks.size() 
              << " tracks" << std::endl;
    return count;
}

bool MusicLibrary::getTrack(int64_t id, TrackInfo& track) {
    if (!is_open_) return false;

    const char* sql = R"(
        SELECT id, file_path, title, artist, album, year, duration_ms,
               artist_id, album_id, play_count, last_played, added_date, is_favorite
        FROM tracks WHERE id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        track.id = sqlite3_column_int64(stmt, 0);
        track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        track.year = sqlite3_column_int(stmt, 5);
        track.duration_ms = sqlite3_column_int64(stmt, 6);
        track.artist_id = sqlite3_column_int64(stmt, 7);
        track.album_id = sqlite3_column_int64(stmt, 8);
        track.play_count = sqlite3_column_int(stmt, 9);
        track.last_played = sqlite3_column_int64(stmt, 10);
        track.added_date = sqlite3_column_int64(stmt, 11);
        track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
        found = true;
    }

    sqlite3_finalize(stmt);
    return found;
}

std::vector<TrackInfo> MusicLibrary::getAllTracks(int limit) {
    std::vector<TrackInfo> tracks;
    if (!is_open_) return tracks;

        std::string sql = R"(
            SELECT id, file_path, title, artist, album, year, duration_ms,
                   artist_id, album_id, play_count, last_played, added_date, is_favorite
            FROM tracks
            WHERE COALESCE(bad_flag, 0) = 0
            ORDER BY title
        )";
    
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return tracks;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TrackInfo track;
        track.id = sqlite3_column_int64(stmt, 0);
        track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        track.year = sqlite3_column_int(stmt, 5);
        track.duration_ms = sqlite3_column_int64(stmt, 6);
        track.artist_id = sqlite3_column_int64(stmt, 7);
        track.album_id = sqlite3_column_int64(stmt, 8);
        track.play_count = sqlite3_column_int(stmt, 9);
        track.last_played = sqlite3_column_int64(stmt, 10);
        track.added_date = sqlite3_column_int64(stmt, 11);
        track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
        tracks.push_back(track);
    }

    sqlite3_finalize(stmt);
    return tracks;
}

void MusicLibrary::recordPlay(int64_t track_id) {
    if (!is_open_) return;

    const char* sql = R"(
        UPDATE tracks 
        SET play_count = play_count + 1, last_played = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    time_t now = std::time(nullptr);
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int64(stmt, 2, track_id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool MusicLibrary::setFavorite(int64_t track_id, bool is_favorite) {
    if (!is_open_) return false;

    const char* sql = "UPDATE tracks SET is_favorite = ? WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, is_favorite ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, track_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<TrackInfo> MusicLibrary::getFavorites() {
    std::vector<TrackInfo> tracks;
    if (!is_open_) return tracks;

    const char* sql = R"(
        SELECT id, file_path, title, artist, album, year, duration_ms,
               artist_id, album_id, play_count, last_played, added_date, is_favorite
        FROM tracks WHERE is_favorite = 1 ORDER BY title
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tracks;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TrackInfo track;
        track.id = sqlite3_column_int64(stmt, 0);
        track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        track.year = sqlite3_column_int(stmt, 5);
        track.duration_ms = sqlite3_column_int64(stmt, 6);
        track.artist_id = sqlite3_column_int64(stmt, 7);
        track.album_id = sqlite3_column_int64(stmt, 8);
        track.play_count = sqlite3_column_int(stmt, 9);
        track.last_played = sqlite3_column_int64(stmt, 10);
        track.added_date = sqlite3_column_int64(stmt, 11);
        track.is_favorite = true;
        tracks.push_back(track);
    }

    sqlite3_finalize(stmt);
    return tracks;
}

std::vector<TrackInfo> MusicLibrary::getRecentlyPlayed(int limit) {
    std::vector<TrackInfo> tracks;
    if (!is_open_) return tracks;

    std::string sql = R"(
        SELECT id, file_path, title, artist, album, year, duration_ms,
               artist_id, album_id, play_count, last_played, added_date, is_favorite
        FROM tracks WHERE last_played > 0 
        ORDER BY last_played DESC
        LIMIT )" + std::to_string(limit);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return tracks;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TrackInfo track;
        track.id = sqlite3_column_int64(stmt, 0);
        track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        track.year = sqlite3_column_int(stmt, 5);
        track.duration_ms = sqlite3_column_int64(stmt, 6);
        track.artist_id = sqlite3_column_int64(stmt, 7);
        track.album_id = sqlite3_column_int64(stmt, 8);
        track.play_count = sqlite3_column_int(stmt, 9);
        track.last_played = sqlite3_column_int64(stmt, 10);
        track.added_date = sqlite3_column_int64(stmt, 11);
        track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
        tracks.push_back(track);
    }

    sqlite3_finalize(stmt);
    return tracks;
}

MusicLibrary::Stats MusicLibrary::getStats() {
    Stats stats;
    if (!is_open_) return stats;

    const char* sql = R"(
        SELECT 
            (SELECT COUNT(*) FROM tracks) as track_count,
            (SELECT COUNT(*) FROM albums) as album_count,
            (SELECT COUNT(*) FROM artists) as artist_count,
            (SELECT COUNT(*) FROM playlists) as playlist_count,
            (SELECT SUM(duration_ms) FROM tracks) as total_duration
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return stats;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.total_tracks = sqlite3_column_int(stmt, 0);
        stats.total_albums = sqlite3_column_int(stmt, 1);
        stats.total_artists = sqlite3_column_int(stmt, 2);
        stats.total_playlists = sqlite3_column_int(stmt, 3);
        stats.total_duration_ms = sqlite3_column_int64(stmt, 4);
    }

    sqlite3_finalize(stmt);
    return stats;
}

int64_t MusicLibrary::getOrCreateArtist(const std::string& artist_name) {
    if (artist_name.empty()) return 0;

    // 先查找
    const char* select_sql = "SELECT id FROM artists WHERE name = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, artist_name.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    // 不存在则创建
    const char* insert_sql = "INSERT INTO artists (name) VALUES (?)";
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, artist_name.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            int64_t id = sqlite3_last_insert_rowid(db_);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    return 0;
}

int64_t MusicLibrary::getOrCreateAlbum(const std::string& album_name, 
                                       const std::string& artist_name, 
                                       int year) {
    if (album_name.empty()) return 0;

    // 先查找
    const char* select_sql = "SELECT id FROM albums WHERE name = ? AND artist = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, album_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, artist_name.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    // 不存在则创建
    const char* insert_sql = "INSERT INTO albums (name, artist, year) VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, album_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, artist_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, year);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            int64_t id = sqlite3_last_insert_rowid(db_);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    return 0;
}

// ========== 搜索功能 ==========

std::vector<TrackInfo> MusicLibrary::searchTracks(const SearchCriteria& criteria) {
    std::vector<TrackInfo> results;
    if (!db_) return results;

    std::string sql = "SELECT * FROM tracks WHERE 1=1 AND COALESCE(bad_flag, 0) = 0";
    
    // 构建WHERE条件
    if (!criteria.title.empty()) {
        sql += " AND title LIKE '%" + criteria.title + "%'";
    }
    if (!criteria.artist.empty()) {
        sql += " AND artist LIKE '%" + criteria.artist + "%'";
    }
    if (!criteria.album.empty()) {
        sql += " AND album LIKE '%" + criteria.album + "%'";
    }
    if (criteria.min_year > 0) {
        sql += " AND year >= " + std::to_string(criteria.min_year);
    }
    if (criteria.max_year > 0) {
        sql += " AND year <= " + std::to_string(criteria.max_year);
    }
    if (criteria.favorites_only) {
        sql += " AND is_favorite = 1";
    }
    
    // 排序
    if (!criteria.order_by.empty()) {
        sql += " ORDER BY " + criteria.order_by;
        if (criteria.descending) {
            sql += " DESC";
        }
    }
    
    // 限制数量
    if (criteria.limit > 0) {
        sql += " LIMIT " + std::to_string(criteria.limit);
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackInfo track;
            track.id = sqlite3_column_int64(stmt, 0);
            track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            track.year = sqlite3_column_int(stmt, 5);
            track.duration_ms = sqlite3_column_int64(stmt, 6);
            track.artist_id = sqlite3_column_int64(stmt, 7);
            track.album_id = sqlite3_column_int64(stmt, 8);
            track.play_count = sqlite3_column_int(stmt, 9);
            track.last_played = sqlite3_column_int64(stmt, 10);
            track.added_date = sqlite3_column_int64(stmt, 11);
            track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
            
            results.push_back(track);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

std::vector<TrackInfo> MusicLibrary::getTracksByArtist(const std::string& artist, int limit) {
    std::vector<TrackInfo> results;
    if (!db_ || artist.empty()) return results;

    std::string sql = "SELECT * FROM tracks WHERE artist LIKE ? ORDER BY year, title";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt;
    std::string search_pattern = "%" + artist + "%";
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackInfo track;
            track.id = sqlite3_column_int64(stmt, 0);
            track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            track.year = sqlite3_column_int(stmt, 5);
            track.duration_ms = sqlite3_column_int64(stmt, 6);
            track.artist_id = sqlite3_column_int64(stmt, 7);
            track.album_id = sqlite3_column_int64(stmt, 8);
            track.play_count = sqlite3_column_int(stmt, 9);
            track.last_played = sqlite3_column_int64(stmt, 10);
            track.added_date = sqlite3_column_int64(stmt, 11);
            track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
            
            results.push_back(track);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

std::vector<TrackInfo> MusicLibrary::getTracksByAlbum(const std::string& album, int limit) {
    std::vector<TrackInfo> results;
    if (!db_ || album.empty()) return results;

    std::string sql = "SELECT * FROM tracks WHERE album LIKE ? ORDER BY title";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt;
    std::string search_pattern = "%" + album + "%";
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackInfo track;
            track.id = sqlite3_column_int64(stmt, 0);
            track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            track.year = sqlite3_column_int(stmt, 5);
            track.duration_ms = sqlite3_column_int64(stmt, 6);
            track.artist_id = sqlite3_column_int64(stmt, 7);
            track.album_id = sqlite3_column_int64(stmt, 8);
            track.play_count = sqlite3_column_int(stmt, 9);
            track.last_played = sqlite3_column_int64(stmt, 10);
            track.added_date = sqlite3_column_int64(stmt, 11);
            track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
            
            results.push_back(track);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

std::vector<TrackInfo> MusicLibrary::getMostPlayed(int limit) {
    std::vector<TrackInfo> results;
    if (!db_) return results;

    std::string sql = "SELECT * FROM tracks WHERE play_count > 0 "
                     "ORDER BY play_count DESC, last_played DESC";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackInfo track;
            track.id = sqlite3_column_int64(stmt, 0);
            track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            track.year = sqlite3_column_int(stmt, 5);
            track.duration_ms = sqlite3_column_int64(stmt, 6);
            track.artist_id = sqlite3_column_int64(stmt, 7);
            track.album_id = sqlite3_column_int64(stmt, 8);
            track.play_count = sqlite3_column_int(stmt, 9);
            track.last_played = sqlite3_column_int64(stmt, 10);
            track.added_date = sqlite3_column_int64(stmt, 11);
            track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
            
            results.push_back(track);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

// ========== 播放列表功能 ==========

int64_t MusicLibrary::createPlaylist(const std::string& name, const std::string& description) {
    if (!db_ || name.empty()) return -1;

    const char* sql = "INSERT INTO playlists (name, description, created_date, track_count) "
                     "VALUES (?, ?, ?, 0)";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, std::time(nullptr));
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            int64_t id = sqlite3_last_insert_rowid(db_);
            sqlite3_finalize(stmt);
            std::cout << "[MusicLibrary] Created playlist: " << name << " (ID: " << id << ")" << std::endl;
            return id;
        }
        sqlite3_finalize(stmt);
    }

    return -1;
}

bool MusicLibrary::deletePlaylist(int64_t playlist_id) {
    if (!db_) return false;

    // 先删除播放列表中的曲目
    const char* delete_tracks_sql = "DELETE FROM playlist_tracks WHERE playlist_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, delete_tracks_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, playlist_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 删除播放列表
    const char* delete_playlist_sql = "DELETE FROM playlists WHERE id = ?";
    if (sqlite3_prepare_v2(db_, delete_playlist_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, playlist_id);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    return false;
}

bool MusicLibrary::addTrackToPlaylist(int64_t playlist_id, int64_t track_id, int position) {
    if (!db_) return false;

    // 如果position为-1，添加到末尾
    if (position == -1) {
        const char* count_sql = "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db_, count_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, playlist_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                position = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    const char* sql = "INSERT INTO playlist_tracks (playlist_id, track_id, position, added_date) "
                     "VALUES (?, ?, ?, ?)";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, playlist_id);
        sqlite3_bind_int64(stmt, 2, track_id);
        sqlite3_bind_int(stmt, 3, position);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);

        if (success) {
            // 更新播放列表的曲目计数
            const char* update_sql = "UPDATE playlists SET track_count = track_count + 1 WHERE id = ?";
            if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, playlist_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        return success;
    }

    return false;
}

bool MusicLibrary::removeTrackFromPlaylist(int64_t playlist_id, int64_t track_id) {
    if (!db_) return false;

    const char* sql = "DELETE FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, playlist_id);
        sqlite3_bind_int64(stmt, 2, track_id);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);

        if (success) {
            // 更新播放列表的曲目计数
            const char* update_sql = "UPDATE playlists SET track_count = track_count - 1 WHERE id = ?";
            if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, playlist_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        return success;
    }

    return false;
}

std::vector<TrackInfo> MusicLibrary::getPlaylistTracks(int64_t playlist_id) {
    std::vector<TrackInfo> results;
    if (!db_) return results;

    const char* sql = "SELECT t.* FROM tracks t "
                     "JOIN playlist_tracks pt ON t.id = pt.track_id "
                     "WHERE pt.playlist_id = ? "
                     "ORDER BY pt.position";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, playlist_id);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackInfo track;
            track.id = sqlite3_column_int64(stmt, 0);
            track.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            track.year = sqlite3_column_int(stmt, 5);
            track.duration_ms = sqlite3_column_int64(stmt, 6);
            track.artist_id = sqlite3_column_int64(stmt, 7);
            track.album_id = sqlite3_column_int64(stmt, 8);
            track.play_count = sqlite3_column_int(stmt, 9);
            track.last_played = sqlite3_column_int64(stmt, 10);
            track.added_date = sqlite3_column_int64(stmt, 11);
            track.is_favorite = sqlite3_column_int(stmt, 12) != 0;
            
            results.push_back(track);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

std::vector<PlaylistInfo> MusicLibrary::getAllPlaylists() {
    std::vector<PlaylistInfo> results;
    if (!db_) return results;

    const char* sql = "SELECT * FROM playlists ORDER BY created_date DESC";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PlaylistInfo playlist;
            playlist.id = sqlite3_column_int64(stmt, 0);
            playlist.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            playlist.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            playlist.created_date = sqlite3_column_int64(stmt, 3);
            playlist.track_count = sqlite3_column_int(stmt, 4);
            
            results.push_back(playlist);
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

} // namespace music_player
