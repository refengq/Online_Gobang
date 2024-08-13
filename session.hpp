#pragma once
#include "util.hpp"
typedef enum
{
    UNLOGIN,
    LOGIN
} ss_statu;
class session
{
private:
    uint64_t _ssid;
    uint64_t _uid;
    ss_statu _statu;
    wsserver_t::timer_ptr _tp; // session 关联定时器

public:
    session(uint64_t ssid) : _ssid(ssid), _statu(UNLOGIN)
    {
        DLOG("SESSION %p 创建", this);
    };
    ~session()
    {
        DLOG("SESSION %p 释放", this);
    }
    uint64_t ssid() { return _ssid; }
    void set_statu(ss_statu statu) { _statu = statu; }
    void set_user(uint64_t uid) { _uid = uid; };
    uint64_t get_user() { return _uid; }
    bool is_login() { return _statu == LOGIN; }
    void set_timer(const wsserver_t::timer_ptr &tp) { _tp = tp; }
    wsserver_t::timer_ptr &get_timer() { return _tp; }
};
#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>;
class session_manager
{
private:
    uint64_t _next_ssid;
    std::mutex _mutex;
    std::unordered_map<uint64_t, session_ptr> _session;
    wsserver_t *_server;

public:
    session_manager(wsserver_t *server) : _next_ssid(1), _server(server)
    {
        DLOG("session管理器初始化完成");
    }
    ~session_manager()
    {
        DLOG("session销毁完成");
    }
    // 登录才能操作故简化为登录创建session
    session_ptr cerate_session(uint64_t uid, ss_statu statu)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp(new session(_next_ssid));
        ssp->set_statu(statu);
        ssp->set_user(uid);
        _session.insert(std::make_pair(_next_ssid, ssp));
        _next_ssid++;
        return ssp;
    }
    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if (it == _session.end())
            return session_ptr();
        return it->second;
    }
    void remove_session(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);
    }
    void append_session(const session_ptr &ssp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(ssp->ssid(), ssp)); // 重新插入
    }
    void set_session_expire_time(uint64_t ssid, int ms)
    {
        // websocketpp的定时器完成session生命周期
        // HTTP (登录,注册)有生命周期,指定时间无通信删除
        // websocket长连接 session永久存在
        // 登陆后创建session http 但是进入游戏大厅永久存在
        // 退出大厅时,设置为生命周期
        session_ptr ssp = get_session_by_ssid(ssid);
        if (ssp.get() == nullptr)
            return;
        wsserver_t::timer_ptr tp = ssp->get_timer();
        // 1 永久存在下,设置永久存在
        if (tp.get() == nullptr && ms == SESSION_FOREVER)
            return;
        else if (tp.get() == nullptr && ms != SESSION_FOREVER)
        { // 2 session永久存在下,设置指定时间后被删除
            // 对这个服务器进行绑定
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp); // 标识任务,其实是服务器进行设定
        }
        else if (tp.get() != nullptr && ms == SESSION_FOREVER)
        {
            // 3 session 设置了定时删除 将session设置为永久存在
            // 删除任务会直接执行,其实不是立即取消,可能导致刚添加就删除
            // 所以需要定时器
            tp->cancel();
            ssp->set_timer(wsserver_t::timer_ptr());
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));//服务器
        }
        // 4 定时删除,重置时间
        else if (tp.get() != nullptr && ms != SESSION_FOREVER)
        {
            tp->cancel();//定时器就是删除,ssid sessionp
            // 一个是进行哈希插入映射  一个是session标识  只有一个是真定时
            //cancel会删除掉,之后添加回表,给服务器设置定时任务,给session标识
            ssp->set_timer(wsserver_t::timer_ptr());
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp)); // 使用append
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp);
        }
    }
};