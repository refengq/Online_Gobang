#pragma once
#include "room.hpp"
#include "online.hpp"
#include <list>
#include <condition_variable>
#include <thread>
template <class T>
class match_queue // 阻塞队列
{
private:
    // 用链表不用queue 删除中间元素
    std::list<T> _list;
    // 阻塞消费者,使用时队列元素<2 阻塞
    std::mutex _mutex;
    std::condition_variable _cond; // 条件变量
    /* data */
public:
    // match_queue();
    //~match_queue();
    int size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }
    // 阻塞线程
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        // 在调用 wait() 之前，必须获取一个独占锁（std::unique_lock）并将它传递给 wait() 函数。
        _cond.wait(lock);
    }
    // 出队数据
    bool pop(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_list.empty())
            return false;
        data = _list.front();
        _list.pop_front();
        return true;//默认返回false!!!!
    }
    // 移出指定数据
    void remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
    }
    // 入队并唤醒条件,没必要分离
    void push(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();
    }
};

class matcher
{
private:
    match_queue<uint64_t> _q_normal;
    match_queue<uint64_t> _q_high;
    match_queue<uint64_t> _q_super;
    // 三个队列处理线程
    std::thread _th_normal;
    std::thread _th_high;
    std::thread _th_super;
    room_manager *_rm;
    user_table *_ut;
    online_manager *_om;

private:
    // 三个队列加入用户

    void handle_match(match_queue<uint64_t> &mq)
    {
        while (1)
        {
            // 队列人数大于2
            while (mq.size() < 2)
                mq.wait();//等待,match的条件变量
            // 人数够 出队两个
            uint64_t uid1, uid2;
            bool ret = mq.pop(uid1); // 输出参数
            if (ret == false)
                continue;
            ret = mq.pop(uid2);
            if (ret == false)
            {
                this->add(uid1);
                continue;
            }
            // 校验两个玩家在线,掉线新加队列
            wsserver_t::connection_ptr conn1 = _om->get_conn_from_hall(uid1);
            if (conn1 == nullptr)
            {
                this->add(uid2);
                continue;
            }
            wsserver_t::connection_ptr conn2 = _om->get_conn_from_hall(uid2);
            if (conn2 == nullptr)
            {
                this->add(uid1);
                continue;
            }
            // 创建房间并加入
            auto rp = _rm->create_room(uid1, uid2);
            if (rp.get() == nullptr)
            {
                this->add(uid1);
                this->add(uid2);
                continue;
            }
            // 对玩家响应
            Json::Value resp;
            resp["optype"]="match_success";
            resp["result"]=true;
            std::string body;
            json_util::serialize(resp,body);
            conn1->send(body);//
            conn2->send(body);
        }
    }
    void th_normal_entry()
    {
        return handle_match(_q_normal);
    }
    void th_high_entry()
    {
        return handle_match(_q_high);
    }
    void th_super_entry()
    {
        return handle_match(_q_super);
    }
    /* data */
public:
    matcher(room_manager *rm, user_table *ut, online_manager *om)
        : _rm(rm), _ut(ut), _om(om),
          _th_normal(std::thread(&matcher::th_normal_entry, this)),
          _th_high(std::thread(&matcher::th_high_entry, this)),
          _th_super(std::thread(&matcher::th_super_entry, this))
    {
        DLOG("游戏匹配模块初始化完毕 ");
    }
    // ~matcher();
    bool add(uint64_t uid)
    {
        // 根据分数判定玩家档次添加不同匹配队列
        // 用户id获取信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        // 添加到队列
        if (ret == false)
        {
            DLOG("获取用户信息失败:%d ", uid);
            return false;
        }
        int score = user["score"].asInt();
        if (score < 2000)
        {
            _q_normal.push(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.push(uid);
        }
        else
        {
            _q_super.push(uid);
        }
        return true;
    }
    bool del(uint64_t uid)
    {
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        // 添加到队列
        if (ret == false)
        {
            DLOG("获取用户信息失败:%d ", uid);
            return false;
        }
        int score = user["score"].asInt();
        if (score < 2000)
        {
            _q_normal.remove(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.remove(uid);
        }
        else
        {
            _q_super.remove(uid);
        }
        return true;
    }
};
