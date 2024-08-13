#pragma once
#include "util.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include "matcher.hpp"
#include "matcher.hpp"

typedef wsserver_t::message_ptr message_ptr;
class gobang_server
{
private:
    wsserver_t _wssrv;
    std::string _web_root; // 静态资源根目录
    user_table _ut;
    online_manager _om;
    room_manager _rm;
    session_manager _sm;
    matcher _mm;
    void file_handler(wsserver_t::connection_ptr &conn) // 网页请求
    {
        // 1 路径uri
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        // 2 root_pathname+uri
        std::string realpath = _web_root + uri;
        // 3 默认首页 login.html
        if (realpath.back() == '/')
        {
            realpath = _web_root + "register.html"; // 返回首页
        }
        // 4 文件读取  // 4.1 返回404
        Json::Value resp_json;
        std::string body;
        bool ret = file_util::read(realpath, body);
        if (ret == false)
        {
            body += "<html><head><meta charset=' UTF - 8 '/></head><body>";
            body += "<h1>NOT FOUND </h1>";
            body += "</body>";
            conn->set_body(body);
            conn->set_status(websocketpp::http::status_code::not_found);
            return;
        }
        conn->set_status(websocketpp::http::status_code::ok);
        conn->set_body(body);
        // 返回响应
    }
    // 设置状态码和result reason的函数
    void http_resp(wsserver_t::connection_ptr &conn, bool result, websocketpp::http::status_code::value code,
                   const std::string &reason)
    {
        Json::Value resp_info;
        resp_info["result"] = result;
        resp_info["reason"] = reason;
        std::string resp_body;
        json_util::serialize(resp_info, resp_body);
        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json"); // 无法识别自己添加
        return;
    }
    void user_reg(wsserver_t::connection_ptr &conn) // 注册请求
    {
        // 1 请求正文
        websocketpp::http::parser::request req = conn->get_request();
        std::string req_body = conn->get_request_body();
        // 2 json反序列化
        Json::Value login_info;
        Json::Value resp_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (ret == false)
        {
            DLOG("反序列化注册信息失败 ");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求正文错误 ");
        }
        // 3 数据库新增用户 考虑失败
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            DLOG("用户名密码不完整 ");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名与密码 ");
        }
        ret = _ut.insert(login_info);
        if (ret == false)
        {
            DLOG("数据库插入数据失败 ");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名已经被占用 ");
        }
        return http_resp(conn, true, websocketpp::http::status_code::ok, "注册成功 ");
    }
    void user_login(wsserver_t::connection_ptr &conn) // 登录请求
    {                                                 // 登录
                                                      // 请求正文 json反序列化->用户名 密码
        websocketpp::http::parser::request req = conn->get_request();
        std::string req_body = conn->get_request_body();
        // 2 json反序列化
        Json::Value login_info;
        Json::Value resp_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (ret == false)
        {
            DLOG("反序列化登录信息失败 ");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求正文错误 ");
        }
        // 正文完整性
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            DLOG("用户名密码不完整 ");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名与密码 ");
        }
        ret = _ut.login(login_info);
        if (ret == false)
        {
            DLOG("用户名密码错误");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名密码错误 ");
        }
        // 失败 400 成功 创建 session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _sm.cerate_session(uid, LOGIN);
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        if (ssp.get() == nullptr)
        {
            DLOG("创建会话错误");
            return http_resp(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话错误 ");
        }
        // 设置响应头部set-cookie,会话id
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        return http_resp(conn, false, websocketpp::http::status_code::ok, "登陆成功 ");
    }
    // cookie: key:value;
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val)
    {
        // Cookie: SSID=xxx; path=xxx;
        // 1 ; 间隔
        std::string sep = "; ";
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, sep, cookie_arr);
        // 2 对分割的字符串找到key val
        for (auto &str : cookie_arr)
        {
            std::vector<std::string> tmp_arr;
            string_util::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2)
                continue;
            if (tmp_arr[0] == "SSID")
            {
                val = tmp_arr[1];
                return true;
            }
        }
        return false;
    }
    void user_info(wsserver_t::connection_ptr &conn) // 用户信息请求
    {

        // 1 获取请求的cookie-ssid
        std::string cookie_str = conn->get_request_header("Cookie");
        // 没cookie返回错误,重新登录
        if (cookie_str.empty())
        {
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到Cookie信息,重新登录 ");
        }
        // 取出ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // 没有ssid信息,重新登录
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到SSID信息,重新登录 ");
        }
        // 2 session里查找会话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        { // 登录过期  没找到session  重新登录
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "登陆过期,重新登录 ");
        }
        uint64_t uid = ssp->get_user();
        // 3 数据库取出信息发送给客户端
        Json::Value user_info;
        ret = _ut.select_by_id(uid, user_info);
        if (ret == false)
        {
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到用户信息,重新登录 ");
        }
        std::string body;
        json_util::serialize(user_info, body);
        conn->set_body(body);
        conn->set_status(websocketpp::http::status_code::ok);
        // conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
        // 4 刷新session时间 ssid
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
    }
    void http_callback(websocketpp::connection_hdl hdl) // hdl作为回调自动传入
    {
        auto conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        std::string method = req.get_method();
        if (method == "POST" && uri == "/reg")
        {
            return user_reg(conn);
        }
        else if (method == "POST" && uri == "/login")
        {
            // DLOG("logining......");
            return user_login(conn);
        }
        else if (method == "GET" && uri == "/info")
        {
            return user_info(conn);
        }
        else
            return file_handler(conn);
    }
    void ws_resp(wsserver_t::connection_ptr &conn, Json::Value &resp)
    {
        std::string body;
        json_util::serialize(resp, body);
        conn->send(body);
    }
    session_ptr get_session_by_cookie(wsserver_t::connection_ptr &conn)
    {
        Json::Value err_resp;
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "找不到Cookie信息,重新登录 ";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        // 取出ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // 没有ssid信息,重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "找不到SSID信息,重新登录 ";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        // 2 session里查找会话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        { // 登录过期  没找到session  重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "登陆过期,重新登录 ";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        return ssp;
    }
    void wsopen_game_hall(wsserver_t::connection_ptr &conn)
    {
        // 大厅长连接
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        // 1 登录验证 判断是否成功登录
        // 2 当前客户重复登录
        Json::Value err_resp;
        if (_om.is_in_game_hall(uid) || _om.is_in_game_room(uid))
        {
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "重复登陆 ";
            err_resp["result"] = false;
            return ws_resp(conn, err_resp);
        }
        // 3 将当前客户端与连接加入游戏大厅
        _om.enter_game_hall(uid, conn);
        // 4 返回成功响应
        Json::Value resp_json;
        resp_json["optype"] = "hall_ready";
        // resp_json["reason"] = "重复登陆 ";
        resp_json["result"] = true;
        ws_resp(conn, resp_json);
        // 5 将session设置为永久存在
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
    }
    void wsopen_game_room(wsserver_t::connection_ptr &conn)
    {
        // session可用性
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        // 用户是否已经在大厅/房间
        Json::Value resp_json;
        if (_om.is_in_game_hall(uid) || _om.is_in_game_room(uid))
        {
            resp_json["optype"] = "room_ready";
            resp_json["reason"] = "重复登陆 ";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        // 当前用户是否已经创建了房间
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "room_ready";
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        // 添加到在线用户的管理里面
        _om.enter_game_room(ssp->get_user(), conn);
        // ssession 设为永久
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
        // 发送回复
        resp_json["optype"] = "room_ready";
        resp_json["result"] = true;
        resp_json["room_id"] = (Json::UInt64)rp->get_room_id();
        resp_json["uid"] = (Json::UInt64)ssp->get_user();
        resp_json["white_id"] = (Json::UInt64)rp->get_white_id();
        resp_json["black_id"] = (Json::UInt64)rp->get_black_id();
        return ws_resp(conn, resp_json);
    }
    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        // 长连接建立后处理函数 (两个 游戏大厅 游戏房间)
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            return wsopen_game_hall(conn);
        }
        else if (uri == "/room")
        {
            return wsopen_game_room(conn);
        }
    }

    void wclose_game_hall(wsserver_t::connection_ptr conn)
    {
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        // 1 登录验证 判断是否成功登录
        _om.exit_game_hall(uid);
        // 2 session 设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
    }
    void wclose_game_room(wsserver_t::connection_ptr conn)
    {
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        // 1 登录验证 判断是否成功登录
        _om.exit_game_room(uid);
        // 2 session 设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        // 将玩家从游戏房间退出要计算胜负,房间没人就销毁
        _rm.remove_room_user(uid);
    }
    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        // websocket连接断开前处理 大厅 房间
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            return wclose_game_hall(conn);
        }
        else if (uri == "/room")
        {
            wclose_game_room(conn);
        }
    }
    void wmsg_game_hall(wsserver_t::connection_ptr &conn, message_ptr &msg)
    {
        // 身份验证 当前客户端玩家
        std::string resp_body;
        Json::Value resp_json;
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 对战匹配  停止匹配
        // 获取请求
        std::string req_body = msg->get_payload();
        Json::Value req_json;
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret == false)
        {
            resp_json["optype"] = "解析失败";
            // resp_json["reason"] = "重复登陆 ";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        // 处理请求
        auto uid = ssp->get_user();
        if (!req_json["optype"].isNull() && req_json["optype"] == "match_start")
        {
            // 开始 添加到匹配队列
            _mm.add(uid);
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"] == "match_stop")
        {
            // 停止 从匹配队列移出
            _mm.del(uid);
            resp_json["optype"] = "match_stop";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else
        {
            resp_json["optype"] = "未知请求";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
    }
    void wmsg_game_room(wsserver_t::connection_ptr &conn, message_ptr &msg)
    {
        // session验证
        auto ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        // 用户是否已经在大厅/房间
        Json::Value resp_json;
        // 当前用户是否已经创建了房间
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        // 获取客户端房间信息
        // 消息反序列化
        Json::Value req_json;
        std::string req_body = msg->get_payload();
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret == false)
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "请求解析失败";
            resp_json["result"] = false;
            DLOG("房间-反序列化请求失败");
            return ws_resp(conn, resp_json);
        }
        DLOG("房间：收到房间请求，开始处理....");
        // 通过房间模块进行消息请求的处理
        return rp->handle_request(req_json);
    }
    void wsmsg_callback(websocketpp::connection_hdl hdl, message_ptr msg)
    {
        // 大厅处理
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            return wmsg_game_hall(conn, msg);
        }
        else if (uri == "/room")
        {
            return wmsg_game_room(conn, msg);
        }
    }

public:
    // 成员初始化,回调函数设置
    gobang_server(
        const std::string &host,
        const std::string &username,
        const std::string &password,
        const std::string &dbname,
        uint16_t port = 3306,
        const std::string &webroot = "./wwwroot/") : _web_root(webroot), _ut(host, username, password, dbname, port),
                                                     _rm(&_ut, &_om), _sm(&_wssrv), _mm(&_rm, &_ut, &_om)
    {
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        _wssrv.init_asio();
        _wssrv.set_reuse_addr(true);
        _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
        _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        _wssrv.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        _wssrv.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
    }
    void start(int port)
    {
        _wssrv.listen(port);
        _wssrv.start_accept();
        _wssrv.run();
    }
};
