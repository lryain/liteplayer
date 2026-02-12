#include "LitePlayerWrapper.h"
#include "MusicPlayerTypes.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

using namespace music_player;

static bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received." << std::endl;
    g_running = false;
}

void printState(PlayState state) {
    switch (state) {
        case PlayState::Idle:
            std::cout << "[State] IDLE" << std::endl;
            break;
        case PlayState::Loading:
            std::cout << "[State] LOADING" << std::endl;
            break;
        case PlayState::Playing:
            std::cout << "[State] PLAYING" << std::endl;
            break;
        case PlayState::Paused:
            std::cout << "[State] PAUSED" << std::endl;
            break;
        case PlayState::Stopped:
            std::cout << "[State] STOPPED" << std::endl;
            break;
        case PlayState::Error:
            std::cout << "[State] ERROR" << std::endl;
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <music_file_or_directory>" << std::endl;
        return 1;
    }
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    
    std::string path = argv[1];
    
    std::cout << "=== MusicPlayerEngine Basic Test ===" << std::endl;
    std::cout << "Input: " << path << std::endl;
    
    // 创建播放器
    LitePlayerWrapper player;
    
    // 设置状态回调
    player.setStateCallback([](PlayState state, int error_code) {
        printState(state);
        if (state == PlayState::Error) {
            std::cout << "Error code: " << error_code << std::endl;
        }
    });
    
    // 初始化
    if (!player.initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return 1;
    }
    
    // 加载文件或播放列表
    if (!player.loadFile(path)) {
        std::cerr << "Failed to load: " << path << std::endl;
        return 1;
    }
    
    // 等待准备完成
    std::cout << "Waiting for player to be ready..." << std::endl;
    for (int i = 0; i < 50 && player.getState() != PlayState::Loading; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (player.getState() == PlayState::Error) {
        std::cerr << "Failed to prepare" << std::endl;
        return 1;
    }
    
    // 开始播放
    std::cout << "Starting playback..." << std::endl;
    if (!player.start()) {
        std::cerr << "Failed to start playback" << std::endl;
        return 1;
    }
    
    // 播放循环
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  [Space] - Pause/Resume" << std::endl;
    std::cout << "  n - Next" << std::endl;
    std::cout << "  p - Previous" << std::endl;
    std::cout << "  s - Stop" << std::endl;
    std::cout << "  q - Quit" << std::endl;
    std::cout << std::endl;
    
    bool paused = false;
    
    while (g_running && player.getState() != PlayState::Stopped) {
        // 显示进度
        int position = player.getPosition();
        int duration = player.getDuration();
        
        if (position >= 0 && duration > 0) {
            std::cout << "\r[Progress] " << position / 1000 << "s / " 
                      << duration / 1000 << "s"
                      << "  [" << (position * 100 / duration) << "%]" << std::flush;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 检查是否播放完成
        if (player.getState() == PlayState::Stopped) {
            std::cout << "\n[Playback completed]" << std::endl;
            break;
        }
    }
    
    std::cout << "\n\nStopping player..." << std::endl;
    player.stop();
    
    std::cout << "Test completed!" << std::endl;
    return 0;
}
