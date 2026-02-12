// test_playback_controller.cpp
// PlaybackController 集成测试

#include "PlaybackController.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

using namespace music_player;
using namespace std::chrono_literals;

// 全局变量用于信号处理
static bool g_running = true;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\nReceived Ctrl+C, stopping..." << std::endl;
        g_running = false;
    }
}

void printProgress(const PlaybackController& controller) {
    int pos = controller.getPosition() / 1000;
    int dur = controller.getDuration() / 1000;
    int progress = dur > 0 ? (pos * 100 / dur) : 0;
    
    std::cout << "\r[" << std::setw(2) << controller.getCurrentTrackIndex() + 1 
              << "/" << controller.getPlaylistSize() << "] "
              << controller.getCurrentTrack().title << "  "
              << std::setw(3) << pos << "s / " << std::setw(3) << dur << "s  ["
              << std::setw(3) << progress << "%]" << std::flush;
}

void eventHandler(PlayerEvent event, const std::string& info) {
    switch (event) {
        case PlayerEvent::TrackStarted:
            std::cout << "\n🎵 Track started: " << info << std::endl;
            break;
        case PlayerEvent::TrackEnded:
            std::cout << "\n✓ Track ended: " << info << std::endl;
            break;
        case PlayerEvent::PlaylistEnded:
            std::cout << "\n🏁 Playlist ended: " << info << std::endl;
            g_running = false;
            break;
        case PlayerEvent::ErrorOccurred:
            std::cerr << "\n❌ Error: " << info << std::endl;
            break;
        case PlayerEvent::StateChanged:
            // 状态变化太频繁，不打印
            break;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== PlaybackController Integration Test ===" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path> [play_mode]" << std::endl;
        std::cerr << "Play modes: 0=Sequential, 1=LoopAll, 2=Random, 3=SingleLoop" << std::endl;
        std::cerr << "Example: " << argv[0] << " ~/Music/test 0" << std::endl;
        return 1;
    }
    
    std::string path = argv[1];
    PlayMode mode = PlayMode::Sequential;
    
    if (argc >= 3) {
        int modeInt = std::atoi(argv[2]);
        if (modeInt >= 0 && modeInt <= 3) {
            mode = static_cast<PlayMode>(modeInt);
        }
    }
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    
    // 创建控制器
    PlaybackController controller;
    
    // 设置事件回调
    controller.setEventCallback(eventHandler);
    
    // 初始化
    if (!controller.initialize()) {
        std::cerr << "❌ Failed to initialize controller" << std::endl;
        return 1;
    }
    
    // 加载播放列表
    if (!controller.loadPlaylist(path)) {
        std::cerr << "❌ Failed to load playlist" << std::endl;
        return 1;
    }
    
    // 设置播放模式
    controller.setPlayMode(mode);
    std::cout << "Play mode: " << static_cast<int>(mode) << std::endl;
    std::cout << "Loaded " << controller.getPlaylistSize() << " tracks" << std::endl;
    
    // 开始播放
    std::cout << "\nStarting playback..." << std::endl;
    if (!controller.play()) {
        std::cerr << "❌ Failed to start playback" << std::endl;
        return 1;
    }
    
    std::cout << "\nControls:" << std::endl;
    std::cout << "  [Space] - Pause/Resume" << std::endl;
    std::cout << "  n - Next track" << std::endl;
    std::cout << "  p - Previous track" << std::endl;
    std::cout << "  s - Stop" << std::endl;
    std::cout << "  q/Ctrl+C - Quit" << std::endl;
    std::cout << std::endl;
    
    // 主循环
    while (g_running) {
        PlayState state = controller.getState();
        
        if (state == PlayState::Playing) {
            printProgress(controller);
        } else if (state == PlayState::Stopped || state == PlayState::Error) {
            // 如果是单曲循环模式，停止意味着列表结束
            if (mode != PlayMode::SingleLoop) {
                std::cout << "\n\nPlayback stopped" << std::endl;
                break;
            }
        }
        
        // 简单的键盘输入处理（非阻塞需要更复杂的实现，这里用简单版本）
        std::this_thread::sleep_for(100ms);
    }
    
    // 清理
    controller.stop();
    std::cout << "\n\nTest completed" << std::endl;
    
    return 0;
}
