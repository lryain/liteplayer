#!/usr/bin/env python3
"""
MusicPlayerEngine CLI 测试工具

用于测试 MusicPlayerEngine 的 ZMQ 接口
"""

import zmq
import json
import time
import argparse
import sys
from datetime import datetime

class MusicPlayerCLI:
    def __init__(self):
        self.context = zmq.Context()
        
        # 命令发布socket
        self.cmd_socket = self.context.socket(zmq.PUB)
        self.cmd_socket.connect("ipc:///tmp/music_player_cmd.sock")
        
        # 事件订阅socket  
        self.event_socket = self.context.socket(zmq.SUB)
        self.event_socket.connect("ipc:///tmp/music_player_event.sock")
        self.event_socket.setsockopt_string(zmq.SUBSCRIBE, "music.event.")
        
        # 等待连接建立
        time.sleep(0.5)
        
    def send_command(self, topic, cmd):
        """发送命令"""
        message = json.dumps(cmd)
        full_msg = f"{topic} {message}"
        self.cmd_socket.send_string(full_msg)
        print(f"[发送] {topic}")
        print(f"  {json.dumps(cmd, ensure_ascii=False, indent=2)}")
        
    def listen_events(self, duration=None):
        """监听事件"""
        print("开始监听事件...")
        start_time = time.time()
        
        try:
            while True:
                if duration and (time.time() - start_time) > duration:
                    break
                    
                try:
                    msg = self.event_socket.recv_string(zmq.NOBLOCK)
                    parts = msg.split(' ', 1)
                    topic = parts[0]
                    data = json.loads(parts[1])
                    
                    timestamp = datetime.fromtimestamp(data.get('timestamp', time.time()))
                    print(f"\n[{timestamp.strftime('%H:%M:%S')}] {topic}")
                    print(f"  {json.dumps(data, ensure_ascii=False, indent=2)}")
                    
                except zmq.Again:
                    time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n停止监听")
            
    # ========== 播放控制命令 ==========
    
    def cmd_play(self, track_id=None):
        """播放"""
        cmd = {"cmd": "play"}
        if track_id:
            cmd["params"] = {"track_id": int(track_id)}
        self.send_command("music.cmd.control", cmd)
        
    def cmd_pause(self):
        """暂停"""
        self.send_command("music.cmd.control", {"cmd": "pause"})
        
    def cmd_resume(self):
        """继续"""
        self.send_command("music.cmd.control", {"cmd": "resume"})
        
    def cmd_stop(self):
        """停止"""
        self.send_command("music.cmd.control", {"cmd": "stop"})
        
    def cmd_next(self):
        """下一首"""
        self.send_command("music.cmd.control", {"cmd": "next"})
        
    def cmd_prev(self):
        """上一首"""
        self.send_command("music.cmd.control", {"cmd": "prev"})
        
    def cmd_seek(self, position_ms):
        """跳转"""
        cmd = {
            "cmd": "seek",
            "params": {"position_ms": int(position_ms)}
        }
        self.send_command("music.cmd.control", cmd)
        
    # ========== 播放列表命令 ==========
    
    def cmd_set_playlist(self, playlist_id=None, track_ids=None):
        """设置播放列表"""
        cmd = {"cmd": "set_playlist", "params": {}}
        if playlist_id:
            cmd["params"]["playlist_id"] = int(playlist_id)
        elif track_ids:
            cmd["params"]["track_ids"] = [int(x) for x in track_ids.split(',')]
        self.send_command("music.cmd.playlist", cmd)
        
    def cmd_add_track(self, track_id, position=None):
        """添加曲目"""
        cmd = {
            "cmd": "add_track",
            "params": {"track_id": int(track_id)}
        }
        if position:
            cmd["params"]["position"] = int(position)
        self.send_command("music.cmd.playlist", cmd)
        
    def cmd_remove_track(self, track_id):
        """移除曲目"""
        cmd = {
            "cmd": "remove_track",
            "params": {"track_id": int(track_id)}
        }
        self.send_command("music.cmd.playlist", cmd)
        
    def cmd_clear_playlist(self):
        """清空播放列表"""
        self.send_command("music.cmd.playlist", {"cmd": "clear"})
        
    def cmd_shuffle(self):
        """随机打乱"""
        self.send_command("music.cmd.playlist", {"cmd": "shuffle"})
        
    # ========== 播放模式命令 ==========
    
    def cmd_set_mode(self, mode):
        """设置播放模式"""
        valid_modes = ["sequential", "loop_all", "random", "single_loop"]
        if mode not in valid_modes:
            print(f"错误: 无效的播放模式 '{mode}'")
            print(f"可用模式: {', '.join(valid_modes)}")
            return
        cmd = {
            "cmd": "set_play_mode",
            "params": {"mode": mode}
        }
        self.send_command("music.cmd.mode", cmd)
        
    # ========== 查询命令 ==========
    
    def cmd_search(self, keyword, fields=None):
        """搜索曲目"""
        cmd = {
            "cmd": "search_tracks",
            "params": {"keyword": keyword}
        }
        if fields:
            cmd["params"]["fields"] = fields.split(',')
        self.send_command("music.cmd.query", cmd)
        
    def cmd_filter_tags(self, tags, match_all=False):
        """按标签筛选"""
        cmd = {
            "cmd": "filter_by_tags",
            "params": {
                "tags": tags.split(','),
                "match_all": match_all
            }
        }
        self.send_command("music.cmd.query", cmd)
        
    def cmd_get_current(self):
        """获取当前曲目"""
        self.send_command("music.cmd.query", {"cmd": "get_current_track"})
        
    def cmd_get_playlists(self):
        """获取播放列表"""
        self.send_command("music.cmd.query", {"cmd": "get_playlists"})
        
    # ========== 库管理命令 ==========
    
    def cmd_scan_directory(self, path):
        """扫描目录"""
        cmd = {
            "cmd": "scan_directory",
            "params": {"path": path}
        }
        self.send_command("music.cmd.library", cmd)
        
    def cmd_rescan_all(self):
        """重新扫描所有"""
        self.send_command("music.cmd.library", {"cmd": "rescan_all"})
        
    # ========== 标签管理命令 ==========
    
    def cmd_add_tag(self, track_id, tag):
        """添加标签"""
        cmd = {
            "cmd": "add_tag_to_track",
            "params": {
                "track_id": int(track_id),
                "tag": tag
            }
        }
        self.send_command("music.cmd.tag", cmd)
        
    def cmd_remove_tag(self, track_id, tag):
        """移除标签"""
        cmd = {
            "cmd": "remove_tag_from_track",
            "params": {
                "track_id": int(track_id),
                "tag": tag
            }
        }
        self.send_command("music.cmd.tag", cmd)

def main():
    parser = argparse.ArgumentParser(description='MusicPlayerEngine CLI 测试工具')
    subparsers = parser.add_subparsers(dest='command', help='命令')
    
    # 播放控制
    parser_play = subparsers.add_parser('play', help='播放')
    parser_play.add_argument('--track-id', type=int, help='曲目ID')
    
    parser_pause = subparsers.add_parser('pause', help='暂停')
    parser_resume = subparsers.add_parser('resume', help='继续')
    parser_stop = subparsers.add_parser('stop', help='停止')
    parser_next = subparsers.add_parser('next', help='下一首')
    parser_prev = subparsers.add_parser('prev', help='上一首')
    
    parser_seek = subparsers.add_parser('seek', help='跳转')
    parser_seek.add_argument('position', type=int, help='位置(毫秒)')
    
    # 播放列表
    parser_set_pl = subparsers.add_parser('set-playlist', help='设置播放列表')
    parser_set_pl.add_argument('--playlist-id', type=int, help='播放列表ID')
    parser_set_pl.add_argument('--track-ids', help='曲目ID列表(逗号分隔)')
    
    parser_add = subparsers.add_parser('add-track', help='添加曲目')
    parser_add.add_argument('track_id', type=int, help='曲目ID')
    parser_add.add_argument('--position', type=int, help='位置')
    
    parser_remove = subparsers.add_parser('remove-track', help='移除曲目')
    parser_remove.add_argument('track_id', type=int, help='曲目ID')
    
    parser_clear = subparsers.add_parser('clear', help='清空播放列表')
    parser_shuffle = subparsers.add_parser('shuffle', help='随机打乱')
    
    # 播放模式
    parser_mode = subparsers.add_parser('mode', help='设置播放模式')
    parser_mode.add_argument('mode', choices=['sequential', 'loop_all', 'random', 'single_loop'])
    
    # 查询
    parser_search = subparsers.add_parser('search', help='搜索曲目')
    parser_search.add_argument('keyword', help='关键词')
    parser_search.add_argument('--fields', help='搜索字段(逗号分隔)')
    
    parser_filter = subparsers.add_parser('filter-tags', help='按标签筛选')
    parser_filter.add_argument('tags', help='标签列表(逗号分隔)')
    parser_filter.add_argument('--match-all', action='store_true', help='匹配所有标签')
    
    parser_current = subparsers.add_parser('current', help='获取当前曲目')
    parser_playlists = subparsers.add_parser('playlists', help='获取播放列表')
    
    # 库管理
    parser_scan = subparsers.add_parser('scan', help='扫描目录')
    parser_scan.add_argument('path', help='目录路径')
    
    parser_rescan = subparsers.add_parser('rescan', help='重新扫描所有')
    
    # 标签管理
    parser_add_tag = subparsers.add_parser('add-tag', help='添加标签')
    parser_add_tag.add_argument('track_id', type=int, help='曲目ID')
    parser_add_tag.add_argument('tag', help='标签')
    
    parser_rm_tag = subparsers.add_parser('remove-tag', help='移除标签')
    parser_rm_tag.add_argument('track_id', type=int, help='曲目ID')
    parser_rm_tag.add_argument('tag', help='标签')
    
    # 监听事件
    parser_listen = subparsers.add_parser('listen', help='监听事件')
    parser_listen.add_argument('--duration', type=int, help='监听时长(秒)')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return
    
    cli = MusicPlayerCLI()
    
    # 执行命令
    if args.command == 'play':
        cli.cmd_play(args.track_id)
    elif args.command == 'pause':
        cli.cmd_pause()
    elif args.command == 'resume':
        cli.cmd_resume()
    elif args.command == 'stop':
        cli.cmd_stop()
    elif args.command == 'next':
        cli.cmd_next()
    elif args.command == 'prev':
        cli.cmd_prev()
    elif args.command == 'seek':
        cli.cmd_seek(args.position)
    elif args.command == 'set-playlist':
        cli.cmd_set_playlist(args.playlist_id, args.track_ids)
    elif args.command == 'add-track':
        cli.cmd_add_track(args.track_id, args.position)
    elif args.command == 'remove-track':
        cli.cmd_remove_track(args.track_id)
    elif args.command == 'clear':
        cli.cmd_clear_playlist()
    elif args.command == 'shuffle':
        cli.cmd_shuffle()
    elif args.command == 'mode':
        cli.cmd_set_mode(args.mode)
    elif args.command == 'search':
        cli.cmd_search(args.keyword, args.fields)
    elif args.command == 'filter-tags':
        cli.cmd_filter_tags(args.tags, args.match_all)
    elif args.command == 'current':
        cli.cmd_get_current()
    elif args.command == 'playlists':
        cli.cmd_get_playlists()
    elif args.command == 'scan':
        cli.cmd_scan_directory(args.path)
    elif args.command == 'rescan':
        cli.cmd_rescan_all()
    elif args.command == 'add-tag':
        cli.cmd_add_tag(args.track_id, args.tag)
    elif args.command == 'remove-tag':
        cli.cmd_remove_tag(args.track_id, args.tag)
    elif args.command == 'listen':
        cli.listen_events(args.duration)
    
    # 如果不是listen命令,短暂监听响应
    if args.command != 'listen':
        print("\n等待响应...")
        cli.listen_events(duration=2)

if __name__ == '__main__':
    main()
