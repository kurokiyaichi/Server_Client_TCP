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
	
//��������С��Ԫ��С
#ifndef RECV_BUFF_SZIE
#define RECV_BUFF_SZIE 10240
#endif // !RECV_BUFF_SZIE

//�ͻ�����������
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
	//��������
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
	//�ڶ�������(��Ϣ������)������˲�����Ҫ���ջ����������ǿͻ��˸���������
	char _szMsgBuf[RECV_BUFF_SZIE * 5] = {};
	//��Ϣ������������β��λ��
	int _lastPos;
};

//�����¼��ӿ�
class NetEvent
{
public:
	//�ͻ��˼����¼�
	virtual void OnNetJoin(ClientSocket* pClient) = 0;
	//�ͻ����뿪�¼�
	virtual void OnNetLeave(ClientSocket* pClient) = 0;
	//�ͻ�����Ϣ�¼�
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

	//�ر�Socket
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
			//�ر��׽���closesocket
			closesocket(sock);
#else
			for (int n = (int)client.size() - 1; n >= 0; n--)
			{
				close(client[n]->sockfd());
				delete client[n];
			}
			// 8 �ر��׽���closesocket
			close(sock);
#endif
			client.clear();
		}
	}

	//�Ƿ�����
	bool isRun()
	{
		return sock != INVALID_SOCKET;
	}

	//����������Ϣ
	//int _nCount = 0;
	bool OnRun()
	{
		while (isRun())
		{
			//�ӻ���������ȡ���ͻ�����
			if (clientBuf.size() > 0)
			{
				lock_guard <mutex>dataLock(_mutex);
				for (auto pClient : clientBuf)
				{
					client.push_back(pClient);
				}
				clientBuf.clear();
			}

			//���û����Ҫ��������ݣ�������
			if (client.empty())
			{
				chrono::milliseconds t(1);
				this_thread::sleep_for(t);
				continue;
			}

			fd_set fdRead;//��������socket�� ���� 
			//������
			FD_ZERO(&fdRead);
			//����������socket�����뼯��
			SOCKET maxSock = client[0]->sockfd();
			for (int n = (int)client.size() - 1; n >= 0; n--)
			{
				FD_SET(client[n]->sockfd(), &fdRead);
				if (maxSock < client[n]->sockfd())
				{
					maxSock = client[n]->sockfd();
				}
			}
			///nfds ��һ������ֵ ��ָfd_set����������������(socket)�ķ�Χ������������
			///���������ļ����������ֵ+1 ��Windows�������������д0
			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, 0, 0, &t); 
			if (ret < 0)
			{
				cout << "select�������..." << endl;
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

	//������
	char _szRecv[RECV_BUFF_SZIE] = {};

	//�������� ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient)
	{
		// ���տͻ�������
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SZIE, 0);
		//cout("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			//cout << "�ͻ���<Socket= " << pClient->sockfd() << " >���˳����������..." << endl;
			return -1;
		}
		//����ȡ�������ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLastPos() + nLen);

		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			//��ʱ�Ϳ���֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (pClient->getLastPos() >= header->dataLength)
			{
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = pClient->getLastPos() - header->dataLength;
				//����������Ϣ
				OnNetMsg(pClient, header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				pClient->setLastPos(nSize);
			}
			else {
				//��Ϣ������ʣ�����ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
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

	//ʹ��serverʱ��Ҫ�ṩһ����ѯ�ͻ�����ʽ�û�������+δ������У���������У������ķ�������
	size_t getClientCount()
	{
		return client.size() + clientBuf.size();
	}

private:
	SOCKET sock;
	vector<ClientSocket*> client;//��ʽ�û�����
	vector<ClientSocket*> clientBuf;//�����û�����
	//���������
	mutex _mutex;
	thread* pThread;
	//�����¼�����
	NetEvent* pNetEvent;
public:
	atomic_int recvCount;
};

class TcpServer :  public NetEvent
{
private:
	SOCKET sock;
	vector<ClientSocket*> client;
	//��Ϣ��������ڲ������߳�)
	vector<CellServer*> cellServer;
	//ÿ����Ϣ��ʱ
	TimeStamp _tTime;
protected:
	//�յ���Ϣ����	
	atomic_int recvCount;
	//�ͻ��˼���
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
	//��ʼ��Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//����Windows socket 2.x����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != sock)
		{
			cout << "<socket= " << (int)sock << ">�رվ�����..." << endl;
			Close();
		}
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == sock)
		{
			cout << "���󣬽���socketʧ��..." << endl;
		}
		else {
			cout << "����socket=< " << (int)sock  << ">�ɹ�..." << endl;
		}
		return sock;
	}

	//��IP�Ͷ˿ں�
	int Bind(const char* ip, unsigned short port)
	{
		//if (INVALID_SOCKET == _sock)
		//{
		//	InitSocket();
		//}
		// bind �����ڽ��ܿͻ������ӵ�����˿�
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
			cout << "����,������˿�< " << port << ">ʧ��..."  << endl;
		}
		else {
			cout << "������˿�< " << port << ">�ɹ���" << endl;
		}
		return ret;
	}

	//�����˿ں�
	int Listen(int i)
	{
		// 3 listen ��������˿�
		int ret = listen(sock, i);
		if (SOCKET_ERROR == ret)
		{
			cout << "socket=<" << sock << " >����, ��������˿�ʧ��..." << endl;
		}
		else {
			cout << "socket=<" << sock << " >��������˿ڳɹ���" << endl;
		}
		return ret;
	}

	//���ܿͻ�������
	SOCKET Accept()
	{
		// 4 accept �ȴ����ܿͻ�������
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
			cout << "socket=<" << (int)sock << " >����, ���ܵ���Ч�ͻ���SOCKET..." << endl;
		}
		else
		{
			//�¿ͻ��˼���ʱ��֤�׶Σ�������
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			//���¿ͻ��˷�����ͻ��������ٵ�CellServer
			addClientToCellServer(new ClientSocket(clientSock));
			//inet_ntoa(clientAddr.sin_addr); //��ȡIP��ַ
		}
		return clientSock;
	}


	void addClientToCellServer(ClientSocket* pClient)
	{
		//���ҿͻ��������ٵ���Ϣ��������߳�
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

	//�����˿�
	void Start(int cellServerCount)
	{
		for (int i = 0; i < cellServerCount; i++)
		{
			auto ser = new CellServer(sock);
			cellServer.push_back(ser);
			//ע�������¼����ն���
			ser->setEventObject(this);
			//������Ϣ�����߳�
			ser->Start();
		}
	}

	//�ر�Socket
	void Close()
	{
		if (sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			// �ر��׽���closesocket
			closesocket(sock);
			//------------
			//���Windows socket����
			WSACleanup();
#else

			//�ر��׽���closesocket
			close(sock);
#endif
		}
	}
	//����������Ϣ
	bool OnRun()
	{
		if (isRun())
		{
			timeForMessage();
			fd_set fdRead;
			//������
			FD_ZERO(&fdRead);
			//����������socket�����뼯��
			FD_SET(sock, &fdRead);
			///nfds ��һ������ֵ ��ָfd_set����������������(socket)�ķ�Χ������������
			///���������ļ����������ֵ+1 ��Windows�������������д0
			timeval t = { 0,10 };
			int ret = select(sock + 1, &fdRead, 0, 0, &t); 
			if (ret < 0)
			{
				cout << "Accept select�������..." << endl;
				Close();
				return false;
			}
			//�ж���������socket���Ƿ��ڼ�����
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
	//�Ƿ�����
	bool isRun()
	{
		return sock != INVALID_SOCKET;
	}



	//���㲢���ÿ���յ���������Ϣ
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

	//ֻ��1���̱߳���������ȫ
	virtual void OnNetJoin(ClientSocket* pClient)
	{
		clientCount++;
	}

	//CellServer 5�����̴߳��� ����ȫ ֻ����1��ʱ��ȫ
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		clientCount--;
	}

	//CellServer 5�����̴߳��� ����ȫ ֻ����1��ʱ��ȫ
	virtual void OnNetMessage(ClientSocket* pClient, DataHeader* header)
	{
		recvCount++;
	}
};

#endif // !_TcpServer_hpp_
