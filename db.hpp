#pragma once
#include "util.hpp"
class user_table
{
private:
    MYSQL *_mysql;
    std::mutex _mutex;

public:
    user_table(const std::string &host,
               const std::string &username,
               const std::string &password,
               const std::string &dbname,
               uint16_t port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, username, password, dbname, port);
        assert(_mysql != NULL);
    }
    ~user_table()
    {
        mysql_util::mysql_destory(_mysql);
        _mysql = NULL;
    }
    bool insert(Json::Value &user) // 有注入攻击风险
    {
#define INSERT_USER "insert user values(null,'%s',password(%s),1000,0,0)"
        if (user["password"].isNull() || user["username"].isNull())//校验
        {
            DLOG("INPUT PASSWORD OR USERNAME");
            return false;
        }
        char sql[4096] = {0};
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("insert user info failed!!\n");
            return false;
        }
        return true;
    } // 注册用户
    bool login(Json::Value &user) // 登录验证
    {
#define LOGIN_USER "select id, score, total_count,\
 win_count from user where username='%s' and password=password('%s');"
        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        // bool ret = mysql_util::mysql_exec(_mysql, sql);
        MYSQL_RES *res = NULL;
        // 查询导入结果集加锁
        {
            std::unique_lock<std::mutex> lock(_mutex); // 自动管理
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("user login failed!!\n");
                return false;
            }
            res = mysql_store_result(_mysql);
            if (NULL == res)
            {
                DLOG("have no login user info!!");
                return false;
            }
        }

        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    bool select_by_name(const std::string &name, Json::Value &user) // 获取信息
    {
#define USER_BY_NAME "select id, score, total_count, win_count from user where username='%s';"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_NAME, name.c_str());
        // bool ret = mysql_util::mysql_exec(_mysql, sql);
        MYSQL_RES *res = NULL;
        // 查询导入结果集加锁
        {
            std::unique_lock<std::mutex> lock(_mutex); // 自动管理
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by name failed!!\n");
                return false;
            }
            res = mysql_store_result(_mysql);
            if (NULL == res)
            {
                DLOG("have no  user info!!");
                return false;
            }
        }

        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["usname"] = name;
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    bool select_by_id(int id, Json::Value &user) // 获取信息
    {
#define USER_BY_ID "select username, score, total_count, win_count from user where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_ID, id);
        // bool ret = mysql_util::mysql_exec(_mysql, sql);
        MYSQL_RES *res = NULL;
        // 查询导入结果集加锁
        {
            std::unique_lock<std::mutex> lock(_mutex); // 自动管理
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by name failed!!\n");
                return false;
            }
            res = mysql_store_result(_mysql);
            if (NULL == res)
            {
                DLOG("have no  user info!!");
                return false;
            }
        }

        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["usname"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    bool win(int id) // 胜利 分数=30 场次,胜场+1
    {
#define USER_WIN "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update win user info failed!!\n");
            return false;
        }
        return true;
    }
    bool lose(int id) //
    {
#define USER_LOSE "update user set score=score-30, total_count=total_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_LOSE, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update lose user info failed!!\n");
            return false;
        }
        return true;
    }
};