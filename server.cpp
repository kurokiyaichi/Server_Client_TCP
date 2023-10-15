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
			printf("�˳�cmdThread�߳�\n");
			break;
		}
		else {
			printf("��֧�ֵ����\n");
		}
	}
}

class MyServer :public TcpServer
{
public:
	//ֻ��1���̱߳���������ȫ
	virtual void OnNetJoin(ClientSocket* pClient)
	{
		clientCount++;
		//cout << "client : " << pClient->sockfd() << "join" << endl;
	}

	//CellServer 5�����̴߳��� ����ȫ ֻ����1��ʱ��ȫ
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		clientCount--;
		cout << "client : " << pClient->sockfd() << "exit" << endl;
	}

	//CellServer 5�����̴߳��� ����ȫ ֻ����1��ʱ��ȫ
	virtual void OnNetMessage(ClientSocket* pClient, DataHeader* header)
	{
		recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//cout("�յ��ͻ���<Socket=%d>����CMD_LOGIN,���ݳ��ȣ�%d,userName=%s PassWord=%s\n", clientSock, login->dataLength, login->userName, login->PassWord);
			//�����ж��û������Ƿ���ȷ�Ĺ���
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//cout("�յ��ͻ���<Socket=%d>����CMD_LOGOUT,���ݳ��ȣ�%d,userName=%s \n", clientSock, logout->dataLength, logout->userName);
			//�����ж��û������Ƿ���ȷ�Ĺ���
			//LogoutResult ret;
			//SendData(clientSock, &ret);
		}
		break;
		default:
		{
			cout << "<socket= " << pClient->sockfd() << " > �յ�δ������Ϣ, ���ݳ��ȣ�" << header->dataLength << endl;
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

	//����UI�߳�
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun)
	{
		server.OnRun();
		//printf("����ʱ�䴦������ҵ��..\n");
	}
	server.Close();
	printf("���˳���\n");
	getchar();
	return 0;
}