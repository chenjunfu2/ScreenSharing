#pragma once

#include "Socket.hpp"
#include <stdio.h>

bool Client(SOCKET& sockClient, char* szIPAddress, int& szIPAnum)
{
	WSADATA wd;
	if (WSAStartup(MAKEWORD(2, 2), &wd))
	{
		printf("加载套接字库失败.\n");
		return false;
	}

	if (LOBYTE(wd.wVersion) != 2 || HIBYTE(wd.wVersion) != 2)
	{
		printf("加载的套接字库版本不一致.\n");
		return false;
	}

	sockClient = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr = inet_addr(szIPAddress);
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(szIPAnum);

	//连接服务器
	if (SOCKET_ERROR == connect(sockClient, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)))
	{
		printf("连接服务器失败.\n");
		return false;
	}

	printf("恭喜你.连接服务器成功.\n");

	return true;
}