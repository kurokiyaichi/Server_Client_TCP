#ifndef _TcpServer_hpp_
#define _TcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE  2508
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include<windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")
#else
	#include<unistd.h> 
	#include<arpa/inet.h>
	#include<string.h>

	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

#include<iostream>
#include<vector>
#include<thread>
#include<mutex>
#include<atomic>
#include<functional>
#include"messageheader.hpp"
#include"timestamp.hpp"

using namespace std;
	
//缓冲区最小单元大小
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 10240
#endif // !RECV_BUFF_SZIE

//客户端数据类型
class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getLastPos()
	{
		return _lastPos;
	}
	void setLastPos(int pos)
	{
		_lastPos = pos;
	}
	//发送数据
	int SendData(DataHeader* header)
	{
		if (header)
		{
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}


private:
	// socket fd_set  file desc set
	SOCKET _sockfd;
	//第二缓冲区(消息缓冲区)，服务端并不需要接收缓冲区，这是客户端该做的事情
	char _szMsgBuf[RECV_BUFF_SZIE * 5] = {};
	//消息缓冲区的数据尾部位置
	int _lastPos;
};

//网络事件接口
class NetEvent
{
public:
	//客户端加入事件
	virtual void OnNetJoin(ClientSocket* pClient) = 0;
	//客户端离开事件
	virtual void OnNetLeave(ClientSocket* pClient) = 0;
	//客户端消息事件
	virtual void OnNetMessage(ClientSocket* pClient,DataHeader* header) = 0;
private:
};

class CellServer
{
public:
	CellServer(SOCKET _sock = INVALID_SOCKET)
	{
		sock = _sock;
		pThread = nullptr;
		pNetEvent = nullptr;
	}
	~CellServer()
	{
		delete pThread;
		Close();
		sock = INVALID_SOCKET;
	}

	void setEventObject(NetEvent* event)
	{
		pNetEvent = event;
	}

	//关闭Socket
	void Close()
	{
		if (sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			for (int n = (int)client.size() - 1; n >= 0; n--)
			{
				closesocket(client[n]->sockfd());
				delete client[n];
			}
			//关闭套接字closesocket
			closesocket(sock);
#else
			for (int n = (int)client.size() - 1; n >= 0; n--)
			{
				close(client[n]->sockfd());
				delete client[n];
			}
			// 8 关闭套节字closesocket
			close(sock);
#endif
			client.clear();
		}
	}

	//是否工作中
	bool isRun()
	{
		return sock != INVALID_SOCKET;
	}

	//处理网络消息
	//int _nCount = 0;
	bool OnRun()
	{
		while (isRun())
		{
			//从缓冲区队列取出客户数据
			if (clientBuf.size() > 0)
			{
				lock_guard <mutex>dataLock(_mutex);
				for (auto pClient : clientBuf)
				{
					client.push_back(pClient);
				}
				clientBuf.clear();
			}

			//如果没有需要处理的数据，就跳过
			if (client.empty())
			{
				chrono::milliseconds t(1);
				this_thread::sleep_for(t);
				continue;
			}

			fd_set fdRead;//描述符（socket） 集合 
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符（socket）加入集合
			SOCKET maxSock = client[0]->sockfd();
			for (int n = (int)client.size() - 1; n >= 0; n--)
			{
				FD_SET(client[n]->sockfd(), &fdRead);
				if (maxSock < client[n]->sockfd())
				{
					maxSock = client[n]->sockfd();
				}
			}
			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, 0, 0, &t); 
			if (ret < 0)
			{
				cout << "select任务结束..." << endl;
				Close();
				return false;
			}

			for (int i = (int)client.size() - 1; i >= 0; i--)
			{
				if (FD_ISSET(client[i]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(client[i]))
					{
						auto iter = client.begin() + i;//std::vector<SOCKET>::iterator
						if (iter != client.end())
						{
							if (pNetEvent)
							{
								pNetEvent->OnNetLeave(client[i]);
							}
							delete client[i];
							client.erase(iter);
						}
					}
				}
			}
		}
	}

	//缓冲区
	char _szRecv[RECV_BUFF_SZIE] = {};

	//接收数据 处理粘包 拆分包
	int RecvData(ClientSocket* pClient)
	{
		// 接收客户端数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SZIE, 0);
		//cout("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			//cout << "客户端<Socket= " << pClient->sockfd() << " >已退出，任务结束..." << endl;
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nLen);

		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = pClient->getLastPos() - header->dataLength;
				//处理网络消息
				OnNetMsg(pClient, header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			else {
				//消息缓冲区剩余数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		pNetEvent->OnNetMessage(pClient, header);
	}

	void addClient(ClientSocket* pClient)
	{
		_mutex.lock();
		clientBuf.push_back(pClient);
		_mutex.unlock();
	}

	void Start()
	{
		pThread = new thread(std::mem_fn(&CellServer::OnRun), this);
	}

	//使用server时需要提供一个查询客户（正式用户队列中+未放入队列，即缓冲队列）数量的方法函数
	size_t getClientCount()
	{
		return client.size() + clientBuf.size();
	}

private:
	SOCKET sock;
	vector<ClientSocket*> client;//正式用户队列
	vector<ClientSocket*> clientBuf;//缓冲用户队列
	//缓冲队列锁
	mutex _mutex;
	thread* pThread;
	//网络事件对象
	NetEvent* pNetEvent;
public:
	atomic_int recvCount;
};

class TcpServer :  public NetEvent
{
private:
	SOCKET sock;
	vector<ClientSocket*> client;
	//消息处理对象（内部创建线程)
	vector<CellServer*> cellServer;
	//每秒消息计时
	TimeStamp _tTime;
protected:
	//收到消息计数	
	atomic_int recvCount;
	//客户端计数
	atomic_int clientCount;

public:
	TcpServer()
	{
		sock = INVALID_SOCKET;
		recvCount = 0;
		clientCount = 0;
	}
	virtual ~TcpServer()
	{
		Close();
	}
	//初始化Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != sock)
		{
			cout << "<socket= " << (int)sock << ">关闭旧连接..." << endl;
			Close();
		}
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == sock)
		{
			cout << "错误，建立socket失败..." << endl;
		}
		else {
			cout << "建立socket=< " << (int)sock  << ">成功..." << endl;
		}
		return sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		//if (INVALID_SOCKET == _sock)
		//{
		//	InitSocket();
		//}
		// bind 绑定用于接受客户端连接的网络端口
		sockaddr_in sin = {};
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip) {
			sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else {
			sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip) {
			sin.sin_addr.s_addr = inet_addr(ip);
		}
		else {
			sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = ::bind(sock, (sockaddr*)&sin, sizeof(sin));
		if (SOCKET_ERROR == ret)
		{
			cout << "错误,绑定网络端口< " << port << ">失败..."  << endl;
		}
		else {
			cout << "绑定网络端口< " << port << ">成功！" << endl;
		}
		return ret;
	}

	//监听端口号
	int Listen(int i)
	{
		// 3 listen 监听网络端口
		int ret = listen(sock, i);
		if (SOCKET_ERROR == ret)
		{
			cout << "socket=<" << sock << " >错误, 监听网络端口失败..." << endl;
		}
		else {
			cout << "socket=<" << sock << " >监听网络端口成功！" << endl;
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET clientSock = INVALID_SOCKET;
#ifdef _WIN32
		clientSock = accept(sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		clientSock = accept(sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
		if (INVALID_SOCKET == clientSock)
		{
			cout << "socket=<" << (int)sock << " >错误, 接受到无效客户端SOCKET..." << endl;
		}
		else
		{
			//新客户端加入时验证阶段，待完善
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			//将新客户端分配给客户数量最少的CellServer
			addClientToCellServer(new ClientSocket(clientSock));
			//inet_ntoa(clientAddr.sin_addr); //获取IP地址
		}
		return clientSock;
	}


	void addClientToCellServer(ClientSocket* pClient)
	{
		//查找客户数量最少的消息处理对象线程
		auto pMinServer = cellServer[0];
		for (auto pCellServer : cellServer)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		OnNetJoin(pClient);
	}

	//启动端口
	void Start(int cellServerCount)
	{
		for (int i = 0; i < cellServerCount; i++)
		{
			auto ser = new CellServer(sock);
			cellServer.push_back(ser);
			//注册网络事件接收对象
			ser->setEventObject(this);
			//启动消息处理线程
			ser->Start();
		}
	}

	//关闭Socket
	void Close()
	{
		if (sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			// 关闭套接字closesocket
			closesocket(sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else

			//关闭套接字closesocket
			close(sock);
#endif
		}
	}
	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			timeForMessage();
			fd_set fdRead;
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符（socket）加入集合
			FD_SET(sock, &fdRead);
			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			timeval t = { 0,10 };
			int ret = select(sock + 1, &fdRead, 0, 0, &t); 
			if (ret < 0)
			{
				cout << "Accept select任务结束..." << endl;
				Close();
				return false;
			}
			//判断描述符（socket）是否在集合中
			if (FD_ISSET(sock, &fdRead))
			{
				FD_CLR(sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;

	}
	//是否工作中
	bool isRun()
	{
		return sock != INVALID_SOCKET;
	}



	//计算并输出每秒收到的网络消息
	void timeForMessage()
	{
		auto t1 = _tTime.getSecond();
		if (t1 >= 1.0)
		{
			cout  << "thread : " << cellServer.size() << "time: " << "clientCount: " << clientCount
				<< t1 << "socket: " << sock << "recvCount: " << (int)(recvCount / t1) << endl;
			recvCount = 0;
			_tTime.update();
		}
	}

	//只有1个线程被触发，安全
	virtual void OnNetJoin(ClientSocket* pClient)
	{
		clientCount++;
	}

	//CellServer 5个多线程触发 不安全 只开启1个时则安全
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		clientCount--;
	}

	//CellServer 5个多线程触发 不安全 只开启1个时则安全
	virtual void OnNetMessage(ClientSocket* pClient, DataHeader* header)
	{
		recvCount++;
	}
};

#endif // !_TcpServer_hpp_
