cmake_minimum_required(VERSION 3.5)
project(MusicPlayerEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 包含目录
set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")
set(LIB_DIR "${TOP_DIR}/example/unix/out")

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${TOP_DIR}/include
    ${TOP_DIR}/thirdparty/sysutils/include
    ${TOP_DIR}/adapter
)

# 源文件
set(ENGINE_SOURCES
    src/core/LitePlayerWrapper.cpp
    src/core/PlaylistManager.cpp
    src/core/PlaybackController.cpp
    src/library/MusicLibrary.cpp
    src/service/ConfigLoader.cpp
    src/service/JsonProtocol.cpp
    src/service/MusicPlayerService.cpp
)

# 查找ZMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

include_directories(${ZMQ_INCLUDE_DIRS})
link_directories(${ZMQ_LIBRARY_DIRS})

# 使用已编译的liteplayer库
link_directories(${LIB_DIR})

# 创建引擎库
add_library(music_player_engine STATIC ${ENGINE_SOURCES})

# 链接（注意顺序很重要）
target_link_libraries(music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# 测试程序
add_executable(test_player tests/test_basic_playback.cpp)
target_link_libraries(test_player 
    music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# PlaylistManager测试
add_executable(test_playlist tests/test_playlist_manager.cpp)
target_link_libraries(test_playlist 
    music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# PlaybackController集成测试
add_executable(test_controller tests/test_playback_controller.cpp)
target_link_libraries(test_controller 
    music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# MusicLibrary数据库测试
add_executable(test_library tests/test_music_library.cpp)
target_link_libraries(test_library 
    music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# Music Player Server (Phase 4)
add_executable(music_player_server src/music_player_server.cpp)
target_link_libraries(music_player_server 
    music_player_engine
    ${LIB_DIR}/libliteplayer_core.a
    ${LIB_DIR}/libliteplayer_adapter.a
    ${LIB_DIR}/libsysutils.a
    ${LIB_DIR}/libmbedtls.a
    ${ZMQ_LIBRARIES}
    sqlite3
    stdc++fs
    pthread
    asound
)

# 安装
install(TARGETS music_player_engine DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
