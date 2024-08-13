/*
typedef lib::weak_ptr<void> connection_hdl;
typedef lib::shared_ptr<lib::asio::steady_timer> timer_ptr;
typedef typename connection_type::ptr connection_ptr;
typedef typename connection_type::message_ptr message_ptr;
using wsserver_t=websocketpp::server<websocketpp::config::asio>;
*/
//在线(大厅,房间)用户管理
#pragma once
#include "util.hpp"
class online_manager
{
private:
    std::mutex _mutex;
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _hall_user;
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _room_user;

public:
    void enter_game_hall(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.insert(std::make_pair(uid, conn));
    }
    void enter_game_room(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.insert(std::make_pair(uid, conn));
    }
    void exit_game_hall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.erase(uid);
    }
    void exit_game_room(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.erase(uid);
    }
    bool is_in_game_hall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(uid);
        if (it == _hall_user.end())
            return false;
        return true;
    }
    bool is_in_game_room(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_user.find(uid);
        if (it == _room_user.end())
            return false;
        return true;
    }
wsserver_t::connection_ptr get_conn_from_hall(uint64_t uid)
{
    bool ret=is_in_game_hall(uid);
    if(ret==false)//返回空指针
    return wsserver_t::connection_ptr();
    return _hall_user.find(uid)->second;
}
wsserver_t::connection_ptr get_conn_from_room(uint64_t uid)
{
    bool ret=is_in_game_room(uid);
    if(ret==false)//返回空指针
    return wsserver_t::connection_ptr();
    return _room_user.find(uid)->second;
}
};
