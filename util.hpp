// 所有的工具类
#pragma once
#include "logger.hpp"
#include <mysql/mysql.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <jsoncpp/json/json.h>
#include <iostream>
#include<unordered_map>
#include <memory>
#include <sstream>
#include<assert.h>
#include <vector>
#include <fstream>
#include<mutex>

using wsserver_t=websocketpp::server<websocketpp::config::asio>;

class mysql_util
{
public:
    static MYSQL *mysql_create(const std::string &host,
                               const std::string &username,
                               const std::string &password,
                               const std::string &dbname,
                               uint16_t port = 3306) // 初始化并返回句柄
    {
        // 初始化句柄
        MYSQL *mysql = mysql_init(NULL);
        if (NULL == mysql)
        {
            ELOG("mysql init failed \n");
            return NULL;
        }
        // 连接服务器
        auto tmp = mysql_real_connect(mysql, host.c_str(), username.c_str(), password.c_str(), dbname.c_str(), port, NULL, 0);
        if (NULL == tmp)
        {
            ELOG("connect mysql server failed:%s\n", mysql_error(mysql));
            mysql_close(mysql);
            return NULL;
        }
        // 设置字符集
        auto utmp = mysql_set_character_set(mysql, "utf8");
        if (0 != utmp)
        {
            ELOG("set_character_set failed:%s\n", mysql_error(mysql));
            mysql_close(mysql); 
            return NULL;
        }
        return mysql;
    }
    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0)
        {
            ELOG("%s\n", sql.c_str());
            ELOG("mysql query failed: %s\n", mysql_error(mysql));
            //mysql_close(mysql);
            return false;
        }
        return true;
    }
    static void mysql_destory(MYSQL *mysql)
    {
        if (NULL != mysql)
            mysql_close(mysql);
    }
};

class json_util
{
public:
    static bool serialize(const Json::Value &root, std::string &str)
    {
        Json::StreamWriterBuilder swb;
        // Json::StreamWriter *sw=swb.newStreamWriter();
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0)
        {
            ELOG("json serialize failed \n");
            return false;
        }
        str = ss.str();
        return true;
    }
    static bool unserialize(const std::string &str, Json::Value &root)
    {
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        std::string err;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
        if (ret == false)
        {
            ELOG("json unserialize failed:%s \n", err.c_str());
            return false;
        }
        return true;
    }
};
class string_util
{
public: // 分割字符串
    static int split(const std::string &src, const std::string &sep,
                     std::vector<std::string> &res)
    {
        size_t pos, idx = 0;
        while (idx < src.size())
        {
            pos = src.find(sep, idx);
            if (pos == std::string::npos) // 结尾符
            {
                res.push_back(src.substr(idx));
                break;
            }
            if (pos == idx) // 1,,,2 连续的分隔符会导致存入空串,下次循环
            {
                idx += sep.size();
                continue;
            }
            res.push_back(src.substr(idx, pos - idx)); // 坐标 长度
            idx = pos + sep.size();
        }
        return res.size();
    }
};

class file_util
{
public:
    static bool read(const std::string &filename, std::string &body)
    {
        std::ifstream ifs(filename, std::ios::binary); // 影音必须二进制
        if (ifs.is_open() == false)
        {
            ELOG("%s file open failed ", filename.c_str());
            return false;
        }
        size_t fsize = 0; // 文件大小
        ifs.seekg(0, std::ios::end);
        fsize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        body.resize(fsize);
        // ifs.read(body.c_str()); const传参不对
        ifs.read(&body[0], fsize);
        if (ifs.good() == false)
        {
            ELOG("%s read file content  failed ", filename.c_str());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
};

/*MYSQL++++++++++++++++++++
// Mysql操作句柄初始化
// 参数说明:
// mysql为空则动态申请句柄空间进⾏初始化
// 返回值: 成功返回句柄指针， 失败返回NULL
MYSQL *mysql_init(MYSQL *mysql)；



// 连接mysql服务器
// 参数说明:
// mysql--初始化完成的句柄
// host---连接的mysql服务器的地址
// user---连接的服务器的⽤⼾名
// passwd-连接的服务器的密码
// db ----默认选择的数据库名称
// port---连接的服务器的端⼝： 默认0是3306端⼝
// unix_socket---通信管道⽂件或者socket⽂件，通常置NULL
// client_flag---客⼾端标志位，通常置0
// 返回值：成功返回句柄指针，失败返回NULL
MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user,
 const char *passwd,const char *db, unsigned int port,
 const char *unix_socket, unsigned long
client_flag);



// 设置当前客⼾端的字符集
// 参数说明:
// mysql--初始化完成的句柄
// csname--字符集名称，通常："utf8"
// 返回值：成功返回0， 失败返回⾮0
int mysql_set_character_set(MYSQL *mysql, const char *csname)



// 选择操作的数据库,不需要
// 参数说明:
// mysql--初始化完成的句柄
// db-----要切换选择的数据库名称
// 返回值：成功返回0， 失败返回⾮0
int mysql_select_db(MYSQL *mysql, const char *db)



// 执⾏sql语句,重要
// 参数说明：
// mysql--初始化完成的句柄
// stmt_str--要执⾏的sql语句
// 返回值：成功返回0， 失败返回⾮0
int mysql_query(MYSQL *mysql, const char *stmt_str)



// 保存查询结果到本地
// 参数说明:
// mysql--初始化完成的句柄
// 返回值：成功返回结果集的指针， 失败返回NULL
MYSQL_RES *mysql_store_result(MYSQL *mysql)




// 获取结果集中的⾏数
// 参数说明:
// result--保存到本地的结果集地址
// 返回值：结果集中数据的条数
uint64_t mysql_num_rows(MYSQL_RES *result)；


// 获取结果集中的列数
// 参数说明:
// result--保存到本地的结果集地址
// 返回值：结果集中每⼀条数据的列数
unsigned int mysql_num_fields(MYSQL_RES *result)


// 遍历结果集, 并且这个接⼝会保存当前读取结果位置，每次获取的都是下⼀条数据
// 参数说明:
// result--保存到本地的结果集地址
// 返回值：实际上是⼀个char **的指针，将每⼀条数据做成了字符串指针数组
// row[0]-第0列 row[1]-第1列 ...
MYSQL_ROW mysql_fetch_row(MYSQL_RES *result)


// 释放结果集
// 参数说明:
// result--保存到本地的结果集地址
void mysql_free_result(MYSQL_RES *result)



// 关闭数据库客⼾端连接，销毁句柄
// 参数说明：
// mysql--初始化完成的句柄
void mysql_close(MYSQL *mysql)

// 获取mysql接⼝执⾏错误原因
// 参数说明：
// mysql--初始化完成的句柄
const char *mysql_error(MYSQL *mysql)
MYSQL++++++++++++++++++++*/

/*JSON+++++++++++++++++++++++++++++++++++++++
class Json::Value
{
 Value &operator=(const Value &other); //Value重载了[]和=，因此所有的赋值和获取数据都可以通过

 Value& operator[](const std::string& key);//简单的⽅式完成 val["name"] = xx";

 Value& operator[](const char* key);

 Value removeMember(const char* key);//移除元素

 const Value& operator[](ArrayIndex index) const; //val["score"][0]

 Value& append(const Value& value);//添加数组元素val["score"].append(88);

 ArrayIndex size() const;//获取数组元素个数 val["score"].size();

 bool isNull(); //⽤于判断是否存在某个字段

 std::string asString() const;//转string string name = val["name"].asString();

 const char* asCString() const;//转char* char *name = val["name"].asCString();

 Int asInt() const;//转int int age = val["age"].asInt();

 float asFloat() const;//转float float weight = val["weight"].asFloat();

 bool asBool() const;//转 bool bool ok = val["ok"].asBool();
};

序列化接⼝
class JSON_API StreamWriter
{
 virtual int write(Value const& root, std::ostream* sout) = 0;
}
class JSON_API StreamWriterBuilder : public StreamWriter::Factory
{
 virtual StreamWriter* newStreamWriter() const;
}

反序列化接⼝
class JSON_API CharReader
{
 virtual bool parse(char const* beginDoc, char const* endDoc, Value* root, std::string* errs) = 0;
}
class JSON_API CharReaderBuilder : public CharReader::Factory
{
 virtual CharReader* newCharReader() const;
}
*/