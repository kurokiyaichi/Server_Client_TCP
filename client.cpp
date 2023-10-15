#include "tcpclient.hpp"
#include<thread>
using namespace std;

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

const int tCount = 5;//发送线程数量
const int cCount = 10000;//客户端数量
TcpClient* client[cCount];//客户端数组

//发送线程
void sendThread(int id)//5线程 ID:1-5
{
	cout << "thread  id : " << id << " start " << endl;
	int allocCount = cCount / tCount;//每个线程分配的客户端
	int begin = (id - 1) * allocCount;
	int end = id * allocCount;
	for (int i = begin; i < end; i++)
	{
		client[i] = new TcpClient();
	}
	for (int i = begin; i < end; i++)
	{	
		client[i]->Connect("127.0.0.1", 8888);
	}

	cout << "thread  id : " << id << "Connect: begin = " << begin  << ", end = " << end << endl;
	chrono::milliseconds t(3000);
	this_thread::sleep_for(t);

	Login login[10];
	for (int i = 0; i < 10; i++)
	{
		strcpy(login[i].userName, "kurokiyaichi");
		strcpy(login[i].pwd, "114514");
	}
	const int nLen = sizeof(login);
	while (g_bRun)
	{
		for (int i = begin; i < end; i++)
		{
			client[i]->SendData(login,nLen);
			client[i]->OnRun();
		}

		//printf("空闲时间处理其它业务..\n");
	}

	for (int i = begin; i < end; i++)
	{
		client[i]->Close();
		delete client[i];
	}
	cout << "thread  id : " << id << " exit " << endl;
}

int main()
{
	//启动UI线程
	thread t1(cmdThread);
	t1.detach();



	for (int i = 0; i < tCount; i++)
	{
		//启动发送线程
		thread t1(sendThread,i+1);
		t1.detach();
	}

	while (g_bRun)
	{
		Sleep(100);
	}

	printf("已退出。\n");
	getchar();
	return 0;
}