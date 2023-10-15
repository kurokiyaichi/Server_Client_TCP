#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <iostream>

#pragma comment(lib,"ws2_32.lib")


int main(int argc,char* argv[])
{
	WORD ver = MAKEWORD(2, 2);
	WSADATA data;
	WSAStartup(ver,&data);

	WSACleanup();
	return 0;
}
