#ifndef _TcpClient_hpp_
#define _TcpClient_hpp_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
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
#include <iostream>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;
#include  "messageheader.hpp"

class TcpClient
{
	SOCKET sock;
	bool isConnect;
public:
	TcpClient()
	{
		sock = INVALID_SOCKET;
		isConnect = false;
	}

	virtual ~TcpClient()
	{
		Close();
	}
	//初始化socket
	void InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != sock)
		{
			cout << "<socket= " << sock <<  ">关闭旧连接..." << endl;
			Close();
		}
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == sock)
		{
			cout << "错误，建立Socket失败..." << endl;
		}
		else {
			cout << "建立Socket=< " << sock << " >成功..." << endl;
		}
	}

	//连接服务器
	int Connect(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == sock)
		{
			InitSocket();
		}
		// 2 连接服务器 connect
		sockaddr_in sin = {};
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		sin.sin_addr.s_addr = inet_addr(ip);
#endif
		cout << "<socket= " << sock << " >正在连接服务器< " << ip << " " << port << " >..." << endl;
		int ret = connect(sock, (sockaddr*)&sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			cout << "<socket= " << sock << " >错误，连接服务器< " << ip << " " << port << " >失败..." << endl;
		}
		else {
			isConnect = true;
			//cout << "<socket= " << sock << ">连接服务器< " << ip << " " << port << " >成功！..." << endl;
		}
		return ret;
	}

	//关闭套节字closesocket
	void Close()
	{
		if (sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			closesocket(sock);
			//清除Windows socket环境
			WSACleanup();
#else
			close(sock);
#endif
			sock = INVALID_SOCKET;
		}
		isConnect = false;
	}

	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(sock, &fdReads);
			timeval t = { 0,0 };
			int ret = select(sock + 1, &fdReads, 0, 0, &t);
			if (ret < 0)
			{
				cout << "<socket= " << sock << " >select任务结束(1)" << endl;
				Close();
				return false;
			}
			if (FD_ISSET(sock, &fdReads))
			{
				FD_CLR(sock, &fdReads);

				if (-1 == recvData(sock))
				{
					cout << "<socket= " << sock << " >select任务结束(2)" << endl;
					Close();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun()
	{
		return sock != INVALID_SOCKET && isConnect;
	}
	//缓冲区最小单元大小
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 102400
#endif // !RECV_BUFF_SZIE
	//消息缓冲区的数据尾部位置
	int lastPos = 0;
	//接收缓冲区
	char szRecv[RECV_BUFF_SZIE] = {};
	//第二缓冲区(消息缓冲区)
	char szMsgBuf[RECV_BUFF_SZIE * 10] = {};

	//接收数据 处理粘包 拆分包
	int recvData(SOCKET clientSock)
	{
		// 5 接收数据
		int nLen = (int)recv(clientSock, szRecv, RECV_BUFF_SZIE, 0);
		//cout("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			cout << "与服务器断开连接，任务结束..." << endl;
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(szMsgBuf + lastPos, szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		lastPos += nLen;
		//判断消息缓冲区的数据长度大于消息头DataHeader长度---少包问题
		while (lastPos >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)szMsgBuf;
			//判断消息缓冲区的数据长度大于消息长度
			if (lastPos >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = lastPos - header->dataLength;
				//处理网络消息
				OnNetMsg(header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(szMsgBuf, szMsgBuf + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				lastPos = nSize;
			}
			else {
				//消息缓冲区剩余数据长度不足一条完整消息---粘包问题
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{

			LoginResult* login = (LoginResult*)header;
			//cout("<socket=%d>收到服务端消息：CMD_LOGIN_RESULT,数据长度：%d\n", sock, login->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logout = (LogoutResult*)header;
			//cout("<socket=%d>收到服务端消息：CMD_LOGOUT_RESULT,数据长度：%d\n", sock, logout->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//cout("<socket=%d>收到服务端消息：CMD_NEW_USER_JOIN,数据长度：%d\n", sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			cout << "<socket = " << sock << ">收到服务端消息：CMD_ERROR,数据长度： " << header->dataLength << endl;
		}
		break;
		default:
		{
			cout << "<socket= " << sock << ">收到未定义消息,数据长度： " << header->dataLength << endl;
		}
		}
	}

	//发送数据
	int SendData(DataHeader* header,const int nLen)
	{
		int ret = SOCKET_ERROR;
		if (isRun() && header)
		{
			ret =  send(sock, (const char*)header, nLen, 0);
			if (ret == SOCKET_ERROR)
			{
				Close();
			}
		}
		return SOCKET_ERROR;
	}
private:

};

#endif