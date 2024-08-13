#pragma once
#include "util.hpp"
#include "db.hpp"
#include "online.hpp"

#define BOARD_ROW 15
#define BOARD_COL 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2
typedef enum
{
    GAME_START,
    GAME_OVER
} room_statu;

class room
{
private:
    uint64_t _room_id;
    room_statu _statu;
    int _player_count; // 0
    uint64_t _white_id;
    uint64_t _black_id;
    user_table *_tb_user;
    online_manager *_online_user;          // 在线管理
    std::vector<std::vector<int>> _board; // 15*15
private:
    bool is_five(int row, int col, int row_off, int col_off, int color)
    {
        int count = 1;
        int find_row = row + row_off;
        int find_col = col + col_off;
        while (find_row >= 0 && find_row < BOARD_ROW && find_col >= 0 && find_col < BOARD_COL && _board[find_row][find_col] == color)
        {
            count++;
            find_row += row_off;
            find_col += col_off;
        }
        // 反向查找
        row_off = -row_off;
        col_off = -col_off;
        find_row = row + row_off;
        find_col = col + col_off;
        while (find_row >= 0 && find_row < BOARD_ROW && find_col >= 0 && find_col < BOARD_COL && _board[find_row][find_col] == color)
        {
            count++;
            find_row += row_off;
            find_col += col_off;
        }
        return count >= 5;
    }
    uint64_t check_win(int row, int col, int color) // 0 1 2
    {
        if (
            is_five(row, col, 0, 1, color) ||
            is_five(row, col, 1, 0, color) ||
            is_five(row, col, -1, 1, color) ||
            is_five(row, col, -1, -1, color))
            return color == CHESS_WHITE ? _white_id : _black_id;
        return 0;
    }

public:
    room(uint64_t room_id, user_table *tb_user, online_manager *online_user)
        : _room_id(room_id), _statu(GAME_START), _player_count(0),
          _tb_user(tb_user), _online_user(online_user), _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0))
    {
        DLOG("%lu 房间创建成功 ", _room_id);
    }
    ~room()
    {
        DLOG("%lu 房间销毁 ", _room_id);
    }
    uint64_t get_room_id() { return _room_id; };
    room_statu get_room_statu() { return _statu; };
    int get_player_count() { return _player_count; }
    void add_white_user(uint64_t uid)
    {
        _white_id = uid;
        _player_count++;
    }
    void add_black_user(uint64_t uid)
    {
        _black_id = uid;
        _player_count++;
    }
    uint64_t get_white_id() { return _white_id; };
    uint64_t get_black_id() { return _black_id; };

    // 处理函数->

    Json::Value handle_chess(Json::Value &req)
    {
        // 判断 房间 用户 走棋位置 胜利
        // 房间
        Json::Value json_resp = req;
        uint64_t room_id = req["room_id"].asUInt64();
        // 用户
        uint64_t cur_uid = req["uid"].asUInt64();
        int cur_row = req["row"].asInt();
        int cur_col = req["col"].asInt();
        if (_online_user->is_in_game_room(_white_id) == false) // 白棋掉线,黑棋赢
        {
            // auto tmp_win_id=(cur_uid == _white_id ? _black_id : _white_id);;
            json_resp["result"] = true;
            json_resp["reason"] = "对方掉线,获得胜利 ";
            json_resp["winner"] = (Json::UInt64)_black_id;
            return json_resp;
        }
        if (_online_user->is_in_game_room(_black_id) == false) // 黑棋掉线,白棋赢
        {
            // auto tmp_win_id=(cur_uid == _white_id ? _black_id : _white_id);;
            json_resp["result"] = true;
            json_resp["reason"] = "对方掉线,获得胜利 ";
            json_resp["winner"] = (Json::UInt64)_white_id;
            return json_resp;
        }
        // 走棋位置
        if (_board[cur_row][cur_col] != 0)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "此位置已有棋子 ";
            return json_resp;
        }
        int cur_color = (cur_uid == _white_id ? CHESS_WHITE : CHESS_BLACK);
        _board[cur_row][cur_col] = cur_color;
        // 胜利
        uint64_t winner_id = check_win(cur_row, cur_col, cur_color);
        if (winner_id != 0)
            json_resp["reason"] = "五子连珠,获得胜利 ";
        json_resp["result"] = true;
        json_resp["winner"] = (Json::UInt64)winner_id;
        return json_resp;
    }
    Json::Value handle_chat(Json::Value &req)
    {
        // 房间
        Json::Value json_resp = req;
        uint64_t room_id = req["room_id"].asUInt64();
        // 消息过滤,测试
        std::string msg = req["message"].asString();
        size_t pos = msg.find("过滤");
        if (pos != std::string::npos)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "消息包含过滤词汇,不进行发送 ";
            return json_resp;
        }
        // 广播
        json_resp["result"] = true;
        return json_resp;
    }
    void handle_exit(uint64_t uid)
    {
        Json::Value json_resp;
        // 下棋中/下棋后 退出  房间人数--
        // 不控制map room?这是已经退出后做处理?
        if (_statu == GAME_START)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "有玩家退出,获得胜利 ";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)uid;
            json_resp["row"] = -1;
            json_resp["col"] = -1;
            json_resp["winner"] = (Json::UInt64)(uid == _white_id ? _black_id : _white_id);
            broadcast(json_resp);
        }
        _player_count--;
        return;
    }
    // 分类请求类型函数,
    void handle_request(Json::Value &req)
    {
        // 校验房间号
        Json::Value json_resp;
        uint64_t room_id = req["room_id"].asUInt64();
        if (room_id != _room_id)
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配 ";
            broadcast(req);
            return;
        }
        // 2 任务分类
        if (req["optype"].asString() == "put_chess")
        {
            json_resp = handle_chess(req);
            uint64_t winner_id = json_resp["winner"].asUInt64();
            if (winner_id != 0) // 胜利处理
            {
               // uint64_t lose_id = _white_id + _black_id - winner_id;
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                _tb_user->win(winner_id);
                _tb_user->lose(loser_id);
                _statu = GAME_OVER;
            }
        }
        else if (req["optype"].asString() == "chat")
        {
            json_resp = handle_chat(req);
        }
        else
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "未知请求 ";
        }
        broadcast(req);
    }
    // 动作广播
    void broadcast(Json::Value &rsp)
    {
        // 序列化 获取连接 发送信息
        std::string body;
        json_util::serialize(rsp, body);
        auto wconn = _online_user->get_conn_from_room(_white_id);
        if (wconn.get() != nullptr)
            wconn->send(body);
        auto bconn = _online_user->get_conn_from_room(_black_id);
        if (bconn.get() != nullptr)
            bconn->send(body);
        return;
    }
};

using room_ptr = std::shared_ptr<room>;
class room_manager
{
private:
    // int _player_count; // 0
    uint64_t _next_rid;
    std::mutex _mutex;
    user_table *_tb_user;
    online_manager *_online_user;
    std::unordered_map<uint64_t, room_ptr> _rooms;
    std::unordered_map<uint64_t, uint64_t> _users; // 用户id->房间id
public:
    // 房间id 计数器
    room_manager(user_table *ut, online_manager *om) : _next_rid(1), _tb_user(ut), _online_user(om)
    {
        DLOG("房间管理模块初始化完成 ");
    }
    ~room_manager()
    {
        DLOG("房间管理模块销毁");
    }
    // 创建房间返回房间的智能指针
    room_ptr create_room(uint64_t uid1, uint64_t uid2)
    {
        // 用户在大厅对战匹配成功创建房间,进入房间新的长连接通信
        // 是否在大厅
        if (_online_user->is_in_game_hall(uid1) == false)
        {
            DLOG("用户: %lu 不在大厅中,创建房间失败 ", uid1);
            return room_ptr();
        }
        if (_online_user->is_in_game_hall(uid2) == false)
        {
            DLOG("用户: %lu 不在大厅中,创建房间失败 ", uid2);
            return room_ptr();
        }
        // 创建房间,添加用户信息
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new room(_next_rid, _tb_user, _online_user));
        rp->add_white_user(uid1);
        rp->add_black_user(uid2);
        // 管理房间信息并返回
        _rooms.insert(std::make_pair(_next_rid, rp));
        _users.insert(std::make_pair(uid1, _next_rid));
        _users.insert(std::make_pair(uid2, _next_rid));
        _next_rid++;
        return rp;
    }
    room_ptr get_room_by_rid(uint64_t rid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _rooms.find(rid);
        if (it == _rooms.end())
            return room_ptr();
        return it->second;
    }
    room_ptr get_room_by_uid(uint64_t uid)
    { // 用户->房间->ptr
        std::unique_lock<std::mutex> lock(_mutex);
        auto tmp = _users.find(uid);
        if (tmp == _users.end())
            return room_ptr();
        auto rid = tmp->second;
        auto it = _rooms.find(rid);
        if (it == _rooms.end())
            return room_ptr();
        return it->second;
    }
    void remove_room(uint64_t rid)
    {
        // 因为智能指针所以移出vector就可以
        //_rooms.erase(rid); 房间id->用户信息
        room_ptr rp = get_room_by_rid(rid);
        if (rp.get() == nullptr)
            return;
        uint64_t uid1 = rp->get_black_id();
        uint64_t uid2 = rp->get_white_id();
        std::unique_lock<std::mutex> lock(_mutex); // 保护user和room
        _users.erase(uid1);
        _users.erase(uid2);
        _rooms.erase(rid);
    }
    // 删除房间指定用户,如果用户空了就删除房间
    void remove_room_user(uint64_t uid)
    {
        room_ptr rp = get_room_by_uid(uid);
        if (rp.get() == nullptr)
            return;
        rp->handle_exit(uid);
        if (rp->get_player_count() == 0) // 没玩家销毁房间
            remove_room(rp->get_room_id());
    }
};

/*
下棋
{
 "optype": "put_chess", // put_chess表⽰当前请求是下棋操作
 "room_id": 222, // room_id 表⽰当前动作属于哪个房间
 "uid": 1, // 当前的下棋操作是哪个⽤⼾发起的
 "row": 3, // 当前下棋位置的⾏号
 "col": 2 // 当前下棋位置的列号
}

{
 "optype": "put_chess",
 "result": false
 "reason": "⾛棋失败具体原因...."
}

{
 "optype": "put_chess",
 "room_id": 222,
 "uid": 1,
 "row": 3,
 "col": 2,
 "result": true,
 "reason": "对⽅掉线，不战⽽胜！" / "对⽅/⼰⽅五星连珠，战⽆敌/虽败犹荣！",
 "winner": 0 // 0-未分胜负， !0-已分胜负 (uid是谁，谁就赢了)
}

聊天
{
 "optype": "chat",
 "room_id": 222,
 "uid": 1,
 "message": "赶紧点"
}

{
 "optype": "chat",
 "result": false
 "reason": "聊天失败具体原因....⽐如有敏感词..."
}
{
 "optype": "chat",
 "result": true,
 "room_id": 222,
 "uid": 1,
 "message": "赶紧点"
}
*/