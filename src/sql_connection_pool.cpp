#include "../include/sql_connection_pool.h"

using namespace std;

//初始化参数
Connection_pool::Connection_pool()
{
	m_CurConn = 0;//当前已使用的连接数量
	m_FreeConn = 0;//当前剩余的连接数量
}

//单例模式，保证只有一个对象
Connection_pool *Connection_pool::GetInstance()
{
	static Connection_pool connPool;
	return &connPool;
}

//构造初始化，数据库地址、端口、登录用户、登录密码、数据库名称等信息
void Connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	//创建数据库连接池，以一个链表维护，并且更新可用数据库数量
	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		connList.push_back(con);
		++m_FreeConn;
	}
	//信号量初始化成最大连接数量
	reserve = sem(m_FreeConn);

	m_MaxConn = m_FreeConn;
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *Connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;

	//取出连接、信号量减一
	reserve.wait();
	
	lock.lock();

	con = connList.front();
	connList.pop_front();

	--m_FreeConn;
	++m_CurConn;

	lock.unlock();
	return con;
}

//释放当前使用的连接
bool Connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	//释放连接信号量加一
	reserve.post();
	return true;
}

//销毁数据库连接池
void Connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		//通过迭代器遍历链表中的数据库，并且关闭
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		//清空连接数以及连接的链表
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int Connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

Connection_pool::~Connection_pool()
{
	DestroyPool();
}

ConnectionRAII::ConnectionRAII(MYSQL **SQL, Connection_pool *connPool){
	*SQL = connPool->GetConnection();
	conRAII = *SQL;
	poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}