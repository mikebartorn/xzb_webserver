#pragma once

#include <stdio.h>
#include <mysql/mysql.h>
#include <error.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "locker.h"
#include "log.h"

using namespace std;

class Connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static Connection_pool *GetInstance();
	
	//初始化参数
	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	Connection_pool();
	~Connection_pool();

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	locker lock;	//互斥量
	list<MYSQL *> connList; //连接池
	sem reserve;	//信号量

public:
	string m_url;			//主机地址
	string m_Port;		 	//数据库端口号
	string m_User;		 	//登陆数据库用户名
	string m_PassWord;	 	//登陆数据库密码
	string m_DatabaseName; 	//使用数据库名
	int m_close_log;		//日志开关
};

class ConnectionRAII{

public:
	ConnectionRAII(MYSQL **con, Connection_pool *connPool);
	~ConnectionRAII();
	
private:
	MYSQL *conRAII;
	Connection_pool *poolRAII;
};