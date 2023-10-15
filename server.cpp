#include "tcpserver.hpp"
#define _CRT_SECURE_NO_WARNINGS
#include<thread>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		cin >> cmdBuf;
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}

class MyServer :public TcpServer
{
public:
	//只有1个线程被触发，安全
	virtual void OnNetJoin(ClientSocket* pClient)
	{
		clientCount++;
		//cout << "client : " << pClient->sockfd() << "join" << endl;
	}

	//CellServer 5个多线程触发 不安全 只开启1个时则安全
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		clientCount--;
		cout << "client : " << pClient->sockfd() << "exit" << endl;
	}

	//CellServer 5个多线程触发 不安全 只开启1个时则安全
	virtual void OnNetMessage(ClientSocket* pClient, DataHeader* header)
	{
		recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//cout("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", clientSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//cout("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", clientSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			//LogoutResult ret;
			//SendData(clientSock, &ret);
		}
		break;
		default:
		{
			cout << "<socket= " << pClient->sockfd() << " > 收到未定义消息, 数据长度：" << header->dataLength << endl;
			//DataHeader ret;
			//SendData(clientSock, &ret);
		}
		break;
		}
	}
private:

};

int main()
{

	TcpServer server;
	server.InitSocket();
	server.Bind(nullptr, 8888);
	server.Listen(5);
	server.Start(5);

	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun)
	{
		server.OnRun();
		//printf("空闲时间处理其它业务..\n");
	}
	server.Close();
	printf("已退出。\n");
	getchar();
	return 0;
}