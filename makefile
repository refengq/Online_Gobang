.PHONY: gobang
gobang:gobang.cc 
	g++ -std=c++11 -g gobang.cc -o gobang -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -L/usr/local/lib -lboost_system -lpthread
