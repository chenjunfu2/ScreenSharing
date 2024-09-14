#include "Client.hpp"
#include "Server.hpp"

#include "Watch.hpp"
#include "Share.hpp"

#include <Windows.h>
#include <stdio.h>

int CaptureAnImage(HWND hWnd);

int main(void)
{	
	printf("Client/Server? (C/S):");
	int choose;
	choose = getchar();

	SOCKET socket;

	if (choose == 'c' || choose == 'C')
	{
		printf("IP Port:");
		char ip[16];//sizeof("255.255.255.255")==16
		int port;
		if (scanf("%15s %d", ip, &port) != 2)
		{
			printf("error!\n");
			return -1;
		}

		if (!Client(socket, ip, port))
		{
			printf("error!\n");
			return -1;
		}
	}
	else if (choose == 's' || choose == 'S')
	{
		printf("Port:");
		int port;
		if (scanf("%d", &port) != 1)
		{
			printf("error!\n");
			return -1;
		}

		if (!Server(socket, port))
		{
			printf("error!\n");
			return -1;
		}
	}
	else
	{
		printf("error!\n");
		return -1;
	}

	//链接成功



	int ret = -1;

	printf("Share/Watch? (S/W):");
	choose = getchar();

	if (choose == 's' || choose == 'S')
	{
		ret = Share(socket);
	}
	else if(choose == 'w' || choose == 'W')
	{
		ret = Watch(socket);
	}
	else
	{
		printf("error!\n");
		return -1;
	}


	system("pause");

	return ret;
}