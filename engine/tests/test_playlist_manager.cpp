// test_playlist_manager.cpp
// PlaylistManager 功能测试

#include "PlaylistManager.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <filesystem>

using namespace music_player;

void printTrack(const Track& track, size_t index) {
    std::cout << "  [" << std::setw(2) << index << "] " 
              << track.title << " (" << track.file_path << ")" << std::endl;
}

void printPlaylist(const PlaylistManager& manager) {
    std::cout << "\n=== Playlist (" << manager.getTrackCount() << " tracks) ===" << std::endl;
    const auto& tracks = manager.getAllTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (i == manager.getCurrentIndex()) {
            std::cout << "▶ ";
        } else {
            std::cout << "  ";
        }
        printTrack(tracks[i], i);
    }
    std::cout << std::endl;
}

void testSequentialMode(PlaylistManager& manager) {
    std::cout << "\n========== Test 1: Sequential Mode ==========" << std::endl;
    
    manager.setPlayMode(PlayMode::Sequential);
    manager.seekTo(0);
    
    std::cout << "Current: " << manager.getCurrentTrack().title << std::endl;
    assert(manager.hasNext());
    assert(!manager.hasPrev());
    
    // 测试next
    int count = 0;
    while (manager.hasNext() && count < 5) {
        manager.next();
        std::cout << "Next: " << manager.getCurrentTrack().title << std::endl;
        count++;
    }
    
    // 测试prev
    std::cout << "\nGoing back..." << std::endl;
    while (manager.hasPrev()) {
        manager.prev();
        std::cout << "Prev: " << manager.getCurrentTrack().title << std::endl;
    }
    
    std::cout << "✅ Sequential mode test passed" << std::endl;
}

void testLoopAllMode(PlaylistManager& manager) {
    std::cout << "\n========== Test 2: Loop All Mode ==========" << std::endl;
    
    manager.setPlayMode(PlayMode::LoopAll);
    manager.seekTo(manager.getTrackCount() - 1);  // 跳到最后一首
    
    std::cout << "Current (last): " << manager.getCurrentTrack().title << std::endl;
    assert(manager.hasNext());  // 循环模式总是有下一首
    
    manager.next();  // 应该回到第一首
    std::cout << "Next (should wrap to first): " << manager.getCurrentTrack().title << std::endl;
    assert(manager.getCurrentIndex() == 0);
    
    manager.prev();  // 应该回到最后一首
    std::cout << "Prev (should wrap to last): " << manager.getCurrentTrack().title << std::endl;
    assert(manager.getCurrentIndex() == manager.getTrackCount() - 1);
    
    std::cout << "✅ Loop all mode test passed" << std::endl;
}

void testSingleLoopMode(PlaylistManager& manager) {
    std::cout << "\n========== Test 3: Single Loop Mode ==========" << std::endl;
    
    manager.setPlayMode(PlayMode::SingleLoop);
    manager.seekTo(3);
    
    size_t originalIndex = manager.getCurrentIndex();
    std::cout << "Current: " << manager.getCurrentTrack().title << std::endl;
    
    manager.next();
    std::cout << "After next: " << manager.getCurrentTrack().title << std::endl;
    assert(manager.getCurrentIndex() == originalIndex);  // 应该保持不变
    
    manager.prev();
    std::cout << "After prev: " << manager.getCurrentTrack().title << std::endl;
    assert(manager.getCurrentIndex() == originalIndex);  // 应该保持不变
    
    std::cout << "✅ Single loop mode test passed" << std::endl;
}

void testRandomMode(PlaylistManager& manager) {
    std::cout << "\n========== Test 4: Random Mode ==========" << std::endl;
    
    manager.setPlayMode(PlayMode::Random);
    manager.seekTo(0);
    
    std::cout << "Current: " << manager.getCurrentTrack().title << std::endl;
    
    // 测试随机播放（播放5首）
    std::cout << "Playing 5 random tracks:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        manager.next();
        std::cout << "  Random track " << i+1 << ": " 
                  << manager.getCurrentTrack().title << std::endl;
    }
    
    std::cout << "✅ Random mode test passed" << std::endl;
}

void testShuffle(PlaylistManager& manager) {
    std::cout << "\n========== Test 5: Shuffle ==========" << std::endl;
    
    std::cout << "Original order:" << std::endl;
    printPlaylist(manager);
    
    std::cout << "After shuffle:" << std::endl;
    manager.shuffle();
    printPlaylist(manager);
    
    std::cout << "After unshuffle:" << std::endl;
    manager.unshuffle();
    printPlaylist(manager);
    
    std::cout << "✅ Shuffle test passed" << std::endl;
}

void testSeekTo(PlaylistManager& manager) {
    std::cout << "\n========== Test 6: Seek To ==========" << std::endl;
    
    manager.setPlayMode(PlayMode::Sequential);
    
    size_t targetIndex = manager.getTrackCount() / 2;
    std::cout << "Seeking to index " << targetIndex << std::endl;
    assert(manager.seekTo(targetIndex));
    assert(manager.getCurrentIndex() == targetIndex);
    std::cout << "Current: " << manager.getCurrentTrack().title << std::endl;
    
    // 测试无效索引
    assert(!manager.seekTo(999));
    
    std::cout << "✅ Seek to test passed" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== PlaylistManager Test Suite ===" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path|file_path>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ~/Music/test" << std::endl;
        return 1;
    }
    
    std::string path = argv[1];
    
    // 创建PlaylistManager
    PlaylistManager manager;
    
    // 加载播放列表
    bool loaded = false;
    if (std::filesystem::is_directory(path)) {
        std::cout << "Loading directory: " << path << std::endl;
        loaded = manager.loadFromDirectory(path);
    } else {
        std::cout << "Loading file: " << path << std::endl;
        loaded = manager.loadFromFile(path);
    }
    
    if (!loaded) {
        std::cerr << "❌ Failed to load playlist" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Loaded " << manager.getTrackCount() << " tracks" << std::endl;
    
    if (manager.isEmpty()) {
        std::cerr << "❌ Playlist is empty" << std::endl;
        return 1;
    }
    
    // 运行测试
    try {
        testSequentialMode(manager);
        testLoopAllMode(manager);
        testSingleLoopMode(manager);
        testRandomMode(manager);
        testShuffle(manager);
        testSeekTo(manager);
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "🎉 All tests passed!" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
