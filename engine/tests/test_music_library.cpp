/*
 * MusicLibrary Test Suite
 * 测试音乐库数据库操作
 */

#include "../include/MusicLibrary.h"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
using namespace music_player;

// 测试计数器
int tests_passed = 0;
int tests_failed = 0;

// 宏定义简化测试代码
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "❌ TEST FAILED: " << msg << " at line " << __LINE__ << std::endl; \
        tests_failed++; \
    } else { \
        std::cout << "✅ PASSED: " << msg << std::endl; \
        tests_passed++; \
    }

// 测试用数据库路径
const std::string TEST_DB = "/tmp/test_music.db";

// 清理测试数据库
void cleanupTestDB() {
    if (fs::exists(TEST_DB)) {
        fs::remove(TEST_DB);
    }
}

// 创建测试Track
Track createTestTrack(
    const std::string& path,
    const std::string& title,
    const std::string& artist = "Test Artist",
    const std::string& album = "Test Album"
) {
    Track track;
    track.file_path = path;
    track.title = title;
    track.artist = artist;
    track.album = album;
    track.year = 2024;
    track.duration_ms = 180000; // 3分钟
    return track;
}

// 测试1: 数据库打开和关闭
void test_open_close() {
    std::cout << "\n=== Test 1: Database Open/Close ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    
    // 测试打开数据库
    bool opened = library.open(TEST_DB);
    TEST_ASSERT(opened, "Database opened successfully");
    
    // 验证数据库文件已创建
    TEST_ASSERT(fs::exists(TEST_DB), "Database file created");
    
    // 测试关闭数据库
    library.close();
    std::cout << "Database closed successfully" << std::endl;
}

// 测试2: 添加和获取单个曲目
void test_add_get_track() {
    std::cout << "\n=== Test 2: Add and Get Track ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 创建测试曲目
    auto track = createTestTrack("/music/song1.mp3", "Song 1");
    
    // 添加曲目
    int64_t id = library.addTrack(track);
    TEST_ASSERT(id > 0, "Track added with valid ID");
    
    // 获取曲目
    TrackInfo retrieved;
    bool found = library.getTrack(id, retrieved);
    TEST_ASSERT(found, "Track retrieved successfully");
    TEST_ASSERT(retrieved.title == "Song 1", "Track title matches");
    TEST_ASSERT(retrieved.artist == "Test Artist", "Track artist matches");
    TEST_ASSERT(retrieved.album == "Test Album", "Track album matches");
    
    library.close();
}

// 测试3: 批量添加曲目
void test_batch_add() {
    std::cout << "\n=== Test 3: Batch Add Tracks ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 创建多个测试曲目
    std::vector<Track> tracks;
    for (int i = 1; i <= 5; i++) {
        tracks.push_back(createTestTrack(
            "/music/song" + std::to_string(i) + ".mp3",
            "Song " + std::to_string(i),
            "Artist " + std::to_string((i % 2) + 1),  // 2个艺术家
            "Album " + std::to_string((i % 3) + 1)    // 3个专辑
        ));
    }
    
    // 批量添加
    int count = library.addTracks(tracks);
    TEST_ASSERT(count == 5, "All 5 tracks added");
    
    // 验证所有曲目
    auto all_tracks = library.getAllTracks();
    TEST_ASSERT(all_tracks.size() == 5, "All tracks retrieved");
    
    library.close();
}

// 测试4: 记录播放和统计
void test_play_stats() {
    std::cout << "\n=== Test 4: Play Statistics ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加曲目
    auto track = createTestTrack("/music/popular.mp3", "Popular Song");
    int64_t id = library.addTrack(track);
    
    // 模拟多次播放
    for (int i = 0; i < 3; i++) {
        library.recordPlay(id);
    }
    
    // 验证播放次数
    TrackInfo updated;
    bool found = library.getTrack(id, updated);
    TEST_ASSERT(found, "Track found after plays");
    TEST_ASSERT(updated.play_count == 3, "Play count is 3");
    TEST_ASSERT(updated.last_played > 0, "Last played timestamp set");
    
    library.close();
}

// 测试5: 收藏功能
void test_favorites() {
    std::cout << "\n=== Test 5: Favorites ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加多个曲目
    std::vector<int64_t> ids;
    for (int i = 1; i <= 3; i++) {
        auto track = createTestTrack(
            "/music/song" + std::to_string(i) + ".mp3",
            "Song " + std::to_string(i)
        );
        ids.push_back(library.addTrack(track));
    }
    
    // 设置第1和第3首为收藏
    library.setFavorite(ids[0], true);
    library.setFavorite(ids[2], true);
    
    // 获取收藏列表
    auto favorites = library.getFavorites();
    TEST_ASSERT(favorites.size() == 2, "2 tracks in favorites");
    TEST_ASSERT(favorites[0].is_favorite, "First favorite marked");
    TEST_ASSERT(favorites[1].is_favorite, "Second favorite marked");
    
    // 取消收藏
    library.setFavorite(ids[0], false);
    favorites = library.getFavorites();
    TEST_ASSERT(favorites.size() == 1, "1 track after unfavoriting");
    
    library.close();
}

// 测试6: 最近播放
void test_recently_played() {
    std::cout << "\n=== Test 6: Recently Played ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加并播放多个曲目
    std::vector<int64_t> ids;
    for (int i = 1; i <= 4; i++) {
        auto track = createTestTrack(
            "/music/song" + std::to_string(i) + ".mp3",
            "Song " + std::to_string(i)
        );
        ids.push_back(library.addTrack(track));
    }
    
    // 播放顺序: Song 2 -> Song 4 -> Song 1
    library.recordPlay(ids[1]);
    library.recordPlay(ids[3]);
    library.recordPlay(ids[0]);
    
    // 获取最近播放（最多2首）
    auto recent = library.getRecentlyPlayed(2);
    TEST_ASSERT(recent.size() == 2, "2 recently played tracks");
    TEST_ASSERT(recent[0].title == "Song 1", "Most recent is Song 1");
    // 注意：由于时间戳精度问题，不严格测试第二首的顺序
    TEST_ASSERT(recent[1].title == "Song 4" || recent[1].title == "Song 2", "Second recent is Song 4 or Song 2");
    
    library.close();
}

// 测试7: 数据库统计
void test_stats() {
    std::cout << "\n=== Test 7: Database Statistics ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加曲目
    std::vector<Track> tracks;
    tracks.push_back(createTestTrack("/music/s1.mp3", "Song 1", "Artist A", "Album X"));
    tracks.push_back(createTestTrack("/music/s2.mp3", "Song 2", "Artist A", "Album Y"));
    tracks.push_back(createTestTrack("/music/s3.mp3", "Song 3", "Artist B", "Album X"));
    library.addTracks(tracks);
    
    // 获取统计
    auto stats = library.getStats();
    TEST_ASSERT(stats.total_tracks == 3, "3 tracks in database");
    TEST_ASSERT(stats.total_artists == 2, "2 artists (A, B)");
    // 注意：Artist B的Album X和Artist A的Album X是不同的专辑
    TEST_ASSERT(stats.total_albums == 3, "3 albums (A+X, A+Y, B+X)");
    TEST_ASSERT(stats.total_duration_ms == 3 * 180000, "Total duration is 9 minutes");
    
    std::cout << "📊 Database Stats:" << std::endl;
    std::cout << "   Tracks: " << stats.total_tracks << std::endl;
    std::cout << "   Artists: " << stats.total_artists << std::endl;
    std::cout << "   Albums: " << stats.total_albums << std::endl;
    std::cout << "   Total Duration: " << (stats.total_duration_ms / 60000) << " min" << std::endl;
    
    library.close();
}

// 测试8: 艺术家和专辑自动管理
void test_artist_album_auto() {
    std::cout << "\n=== Test 8: Auto Artist/Album Management ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加同一艺术家的多首歌
    library.addTrack(createTestTrack("/m/s1.mp3", "Song 1", "Same Artist", "Album A"));
    library.addTrack(createTestTrack("/m/s2.mp3", "Song 2", "Same Artist", "Album A"));
    library.addTrack(createTestTrack("/m/s3.mp3", "Song 3", "Same Artist", "Album B"));
    
    auto stats = library.getStats();
    TEST_ASSERT(stats.total_artists == 1, "Only 1 artist created");
    TEST_ASSERT(stats.total_albums == 2, "2 albums created (A, B)");
    
    library.close();
}

// 测试8.5: 更新和删除曲目
void test_update_delete() {
    std::cout << "\n=== Test 8.5: Update and Delete Track ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加测试曲目
    auto track = createTestTrack("/m/original.mp3", "Original Song", "Artist A", "Album A");
    auto id = library.addTrack(track);
    TEST_ASSERT(id > 0, "Track added successfully");
    
    // 更新曲目信息
    Track updated_track;
    updated_track.file_path = "/m/updated.mp3";
    updated_track.title = "Updated Song";
    updated_track.artist = "Artist B";
    updated_track.album = "Album B";
    updated_track.year = 2025;
    updated_track.duration_ms = 200000;
    
    bool updated = library.updateTrack(id, updated_track);
    TEST_ASSERT(updated, "Track updated successfully");
    
    // 验证更新
    TrackInfo retrieved;
    bool found = library.getTrack(id, retrieved);
    TEST_ASSERT(found, "Updated track found");
    TEST_ASSERT(retrieved.title == "Updated Song", "Title updated");
    TEST_ASSERT(retrieved.artist == "Artist B", "Artist updated");
    TEST_ASSERT(retrieved.album == "Album B", "Album updated");
    TEST_ASSERT(retrieved.year == 2025, "Year updated");
    
    // 删除曲目
    bool deleted = library.deleteTrack(id);
    TEST_ASSERT(deleted, "Track deleted successfully");
    
    // 验证删除
    TrackInfo not_found;
    bool still_exists = library.getTrack(id, not_found);
    TEST_ASSERT(!still_exists, "Track no longer exists after deletion");
    
    library.close();
}

// 测试9: 搜索功能
void test_search() {
    std::cout << "\n=== Test 9: Search Functions ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加测试数据
    library.addTrack(createTestTrack("/m/rock1.mp3", "Rock Song 1", "Rock Band", "Rock Album"));
    library.addTrack(createTestTrack("/m/rock2.mp3", "Rock Song 2", "Rock Band", "Rock Album"));
    library.addTrack(createTestTrack("/m/pop1.mp3", "Pop Song", "Pop Star", "Pop Album"));
    library.addTrack(createTestTrack("/m/jazz1.mp3", "Jazz Song", "Jazz Master", "Jazz Album"));
    
    // 测试按艺术家搜索
    auto rock_tracks = library.getTracksByArtist("Rock Band");
    TEST_ASSERT(rock_tracks.size() == 2, "Found 2 Rock Band tracks");
    
    // 测试按专辑搜索
    auto pop_tracks = library.getTracksByAlbum("Pop Album");
    TEST_ASSERT(pop_tracks.size() == 1, "Found 1 Pop Album track");
    
    // 测试综合搜索
    SearchCriteria criteria;
    criteria.artist = "Rock";
    criteria.limit = 10;
    auto search_results = library.searchTracks(criteria);
    TEST_ASSERT(search_results.size() == 2, "Search found 2 rock tracks");
    
    library.close();
}

// 测试10: 最多播放
void test_most_played() {
    std::cout << "\n=== Test 10: Most Played ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加曲目并模拟播放
    auto id1 = library.addTrack(createTestTrack("/m/s1.mp3", "Song 1"));
    auto id2 = library.addTrack(createTestTrack("/m/s2.mp3", "Song 2"));
    auto id3 = library.addTrack(createTestTrack("/m/s3.mp3", "Song 3"));
    
    // 播放次数：Song 2 (5次), Song 1 (3次), Song 3 (1次)
    for (int i = 0; i < 5; i++) library.recordPlay(id2);
    for (int i = 0; i < 3; i++) library.recordPlay(id1);
    library.recordPlay(id3);
    
    // 获取最多播放的2首
    auto most_played = library.getMostPlayed(2);
    TEST_ASSERT(most_played.size() == 2, "Got top 2 most played");
    TEST_ASSERT(most_played[0].title == "Song 2", "Most played is Song 2");
    TEST_ASSERT(most_played[0].play_count == 5, "Song 2 played 5 times");
    TEST_ASSERT(most_played[1].title == "Song 1", "Second most is Song 1");
    
    library.close();
}

// 测试11: 播放列表管理
void test_playlist() {
    std::cout << "\n=== Test 11: Playlist Management ===" << std::endl;
    
    cleanupTestDB();
    
    MusicLibrary library;
    library.open(TEST_DB);
    
    // 添加曲目
    auto id1 = library.addTrack(createTestTrack("/m/s1.mp3", "Song 1"));
    auto id2 = library.addTrack(createTestTrack("/m/s2.mp3", "Song 2"));
    auto id3 = library.addTrack(createTestTrack("/m/s3.mp3", "Song 3"));
    
    // 创建播放列表
    auto playlist_id = library.createPlaylist("My Playlist", "Test playlist");
    TEST_ASSERT(playlist_id > 0, "Playlist created successfully");
    
    // 添加曲目到播放列表
    bool added1 = library.addTrackToPlaylist(playlist_id, id1);
    bool added2 = library.addTrackToPlaylist(playlist_id, id2);
    bool added3 = library.addTrackToPlaylist(playlist_id, id3);
    TEST_ASSERT(added1 && added2 && added3, "All tracks added to playlist");
    
    // 获取播放列表曲目
    auto playlist_tracks = library.getPlaylistTracks(playlist_id);
    TEST_ASSERT(playlist_tracks.size() == 3, "Playlist has 3 tracks");
    
    // 移除一首曲目
    bool removed = library.removeTrackFromPlaylist(playlist_id, id2);
    TEST_ASSERT(removed, "Track removed from playlist");
    
    playlist_tracks = library.getPlaylistTracks(playlist_id);
    TEST_ASSERT(playlist_tracks.size() == 2, "Playlist now has 2 tracks");
    
    // 获取所有播放列表
    auto playlists = library.getAllPlaylists();
    TEST_ASSERT(playlists.size() == 1, "1 playlist exists");
    TEST_ASSERT(playlists[0].name == "My Playlist", "Playlist name matches");
    
    // 删除播放列表
    bool deleted = library.deletePlaylist(playlist_id);
    TEST_ASSERT(deleted, "Playlist deleted");
    
    playlists = library.getAllPlaylists();
    TEST_ASSERT(playlists.size() == 0, "No playlists after deletion");
    
    library.close();
}

// 主函数
int main(int argc, char* argv[]) {
    std::cout << "╔═══════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   MusicLibrary Test Suite                        ║" << std::endl;
    std::cout << "║   Testing SQLite3 Database Operations            ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════╝" << std::endl;
    
    // 运行所有测试
    test_open_close();
    test_add_get_track();
    test_batch_add();
    test_play_stats();
    test_favorites();
    test_recently_played();
    test_stats();
    test_artist_album_auto();
    test_update_delete();
    test_search();
    test_most_played();
    test_playlist();
    
    // 清理测试数据库
    cleanupTestDB();
    
    // 输出测试结果
    std::cout << "\n╔═══════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Test Results                                    ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════╝" << std::endl;
    std::cout << "✅ Passed: " << tests_passed << std::endl;
    std::cout << "❌ Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "\n🎉 All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n⚠️  Some tests failed!" << std::endl;
        return 1;
    }
}
