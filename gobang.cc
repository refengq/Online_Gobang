#include "util.hpp"
#include"server.hpp"


#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASS "stsql"
#define DBNAME "gobang"

void mysql_test()
{
    MYSQL *mysql = mysql_util::mysql_create(HOST, USER, PASS, DBNAME, PORT);
    const char *sql = "insert stu values(null,'小一',18,53,68,87);";
    auto ret = mysql_util::mysql_exec(mysql, sql);
    mysql_util::mysql_destory(mysql);
}

void json_test()
{
    Json::Value root;
    root["姓名"] = "小明";
    root["年龄"] = 18;
    root["成绩"].append(98);
    root["成绩"].append(88.5);
    root["成绩"].append(78.5);
    std::string body;
    json_util::serialize(root, body);
    DLOG("%s", body.c_str());
    Json::Value val;
    json_util::unserialize(body, val);

    {
        using namespace std;
        cout << "姓名:" << val["姓名"].asString() << endl;
        cout << "年龄:" << val["年龄"].asInt() << endl;
        int sz = val["成绩"].size();
        for (int i = 0; i < sz; i++)
            cout << "成绩:" << val["成绩"][i].asFloat() << endl;
    };
}

void string_test()
{
    using namespace std;
    string str = "123,45,,,,789,0";
    vector<string> res;
    res.push_back("test1");
    string_util::split(str, ",", res);
    for (auto s : res)
    {
        cout << s << endl;
    }
}

void file_test()
{
    using namespace std;
    string finame = "./makefile";
    string body;
    file_util::read(finame, body);
    cout << body;
}

void db_test()
{
    user_table ut(HOST, USER, PASS, DBNAME, PORT);
    Json::Value user;
    // user["username"] = "xiaoming";
    // user["password"]="123456789";
    // user["password"] = "1234567089";
    // ut.insert(user);
    // bool ret = ut.login(user);
    // bool ret=ut.select_by_id(2,user);
    ut.lose(1);
    bool ret = ut.select_by_id(1, user);
    std::string body;
    json_util::serialize(user, body);
    DLOG("%s", body.c_str());
}

void online_test()
{
    online_manager om;
    wsserver_t::connection_ptr conn;
    uint64_t uid = 2;
    om.enter_game_room(uid, conn);
    if (om.is_in_game_room(uid))
        DLOG("in game hall");
    else
        DLOG("not in game hall");
    om.exit_game_room(uid);
    if (om.is_in_game_room(uid))
        DLOG("in game hall");
    else
        DLOG("not in game hall");
}

void room_test()
{
    user_table ut(HOST, USER, PASS, DBNAME, PORT);
    online_manager om;
    
    room r(10,&ut,&om);
}

int main()
{
//     user_table ut(HOST, USER, PASS, DBNAME, PORT);
//     online_manager om;
    
//     room_manager rm(&ut,&om);
//   //room_ptr rp;
//   auto rp=rm.create_room(10,20);
//   //session s;
//   matcher mc(&rm,&ut,&om);
    gobang_server gs(HOST, USER, PASS, DBNAME, PORT);
    gs.start(8085);


}