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
	//��ʼ��socket
	void InitSocket()
	{
#ifdef _WIN32
		//����Windows socket 2.x����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != sock)
		{
			cout << "<socket= " << sock <<  ">�رվ�����..." << endl;
			Close();
		}
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == sock)
		{
			cout << "���󣬽���Socketʧ��..." << endl;
		}
		else {
			cout << "����Socket=< " << sock << " >�ɹ�..." << endl;
		}
	}

	//���ӷ�����
	int Connect(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == sock)
		{
			InitSocket();
		}
		// 2 ���ӷ����� connect
		sockaddr_in sin = {};
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		sin.sin_addr.s_addr = inet_addr(ip);
#endif
		cout << "<socket= " << sock << " >�������ӷ�����< " << ip << " " << port << " >..." << endl;
		int ret = connect(sock, (sockaddr*)&sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			cout << "<socket= " << sock << " >�������ӷ�����< " << ip << " " << port << " >ʧ��..." << endl;
		}
		else {
			isConnect = true;
			//cout << "<socket= " << sock << ">���ӷ�����< " << ip << " " << port << " >�ɹ���..." << endl;
		}
		return ret;
	}

	//�ر��׽���closesocket
	void Close()
	{
		if (sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			closesocket(sock);
			//���Windows socket����
			WSACleanup();
#else
			close(sock);
#endif
			sock = INVALID_SOCKET;
		}
		isConnect = false;
	}

	//����������Ϣ
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
				cout << "<socket= " << sock << " >select�������(1)" << endl;
				Close();
				return false;
			}
			if (FD_ISSET(sock, &fdReads))
			{
				FD_CLR(sock, &fdReads);

				if (-1 == recvData(sock))
				{
					cout << "<socket= " << sock << " >select�������(2)" << endl;
					Close();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//�Ƿ�����
	bool isRun()
	{
		return sock != INVALID_SOCKET && isConnect;
	}
	//��������С��Ԫ��С
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 102400
#endif // !RECV_BUFF_SZIE
	//��Ϣ������������β��λ��
	int lastPos = 0;
	//���ջ�����
	char szRecv[RECV_BUFF_SZIE] = {};
	//�ڶ�������(��Ϣ������)
	char szMsgBuf[RECV_BUFF_SZIE * 10] = {};

	//�������� ����ճ�� ��ְ�
	int recvData(SOCKET clientSock)
	{
		// 5 ��������
		int nLen = (int)recv(clientSock, szRecv, RECV_BUFF_SZIE, 0);
		//cout("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			cout << "��������Ͽ����ӣ��������..." << endl;
			return -1;
		}
		//����ȡ�������ݿ�������Ϣ������
		memcpy(szMsgBuf + lastPos, szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		lastPos += nLen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����---�ٰ�����
		while (lastPos >= sizeof(DataHeader))
		{
			//��ʱ�Ϳ���֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)szMsgBuf;
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (lastPos >= header->dataLength)
			{
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = lastPos - header->dataLength;
				//����������Ϣ
				OnNetMsg(header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(szMsgBuf, szMsgBuf + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				lastPos = nSize;
			}
			else {
				//��Ϣ������ʣ�����ݳ��Ȳ���һ��������Ϣ---ճ������
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{

			LoginResult* login = (LoginResult*)header;
			//cout("<socket=%d>�յ��������Ϣ��CMD_LOGIN_RESULT,���ݳ��ȣ�%d\n", sock, login->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logout = (LogoutResult*)header;
			//cout("<socket=%d>�յ��������Ϣ��CMD_LOGOUT_RESULT,���ݳ��ȣ�%d\n", sock, logout->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//cout("<socket=%d>�յ��������Ϣ��CMD_NEW_USER_JOIN,���ݳ��ȣ�%d\n", sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			cout << "<socket = " << sock << ">�յ��������Ϣ��CMD_ERROR,���ݳ��ȣ� " << header->dataLength << endl;
		}
		break;
		default:
		{
			cout << "<socket= " << sock << ">�յ�δ������Ϣ,���ݳ��ȣ� " << header->dataLength << endl;
		}
		}
	}

	//��������
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