// music_player_server.cpp
// 音乐播放器服务主程序

#include "MusicPlayerService.h"
#include "ConfigLoader.h"
#include <iostream>
#include <signal.h>
#include <atomic>

using namespace music_player;

// 全局服务实例
std::unique_ptr<MusicPlayerService> g_service;
std::atomic<bool> g_should_exit(false);

// 信号处理
void signalHandler(int signal) {
    std::cout << "\n[Server] Received signal " << signal << ", shutting down..." << std::endl;
    g_should_exit = true;
    
    if (g_service) {
        g_service->stop();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Music Player Server" << std::endl;
    std::cout << "  Phase 4 - ZMQ Remote Control" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 获取配置文件路径
    std::string config_file = ConfigLoader::getDefaultConfigPath();
    if (argc > 1) {
        config_file = argv[1];
    }
    
    std::cout << "[Server] Using config file: " << config_file << std::endl;
    
    // 创建服务
    g_service = std::make_unique<MusicPlayerService>();
    
    // 初始化服务
    if (!g_service->initialize(config_file)) {
        std::cerr << "[Server] Failed to initialize service" << std::endl;
        return 1;
    }
    
    // 启动服务
    if (!g_service->start()) {
        std::cerr << "[Server] Failed to start service" << std::endl;
        return 1;
    }
    
    std::cout << "[Server] Service is running. Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;
    
    // 主循环
    while (!g_should_exit && g_service->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 清理
    std::cout << "[Server] Cleaning up..." << std::endl;
    g_service.reset();
    
    std::cout << "[Server] Shutdown complete" << std::endl;
    return 0;
}
