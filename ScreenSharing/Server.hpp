#pragma once

#include "Socket.hpp"
#include <stdio.h>

bool Server(SOCKET& sockServ, int& szIPAnum)
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

	SOCKET sockListen = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//大端小端 h:host, to: 转到 ，n: net ,  s:short
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(szIPAnum);						//0-》1024， 1024以上
	if (SOCKET_ERROR == bind(sockListen, (sockaddr*)&addrSrv, sizeof(sockaddr)))
	{
		printf("绑定套接字失败.\n");
		return false;
	}

	if (SOCKET_ERROR == listen(sockListen, 5)) //5p
	{
		printf("监听失败,请检查网络！\n");
		return false;
	}

	printf("正在监听...\n");

	SOCKADDR_IN addrClient;
	int nLength = sizeof(SOCKADDR);
	sockServ = accept(sockListen, (SOCKADDR*)&addrClient, &nLength);
	printf("IP:%s端口:%d  已连接至服务器\n", inet_ntoa(addrClient.sin_addr), addrClient.sin_port);


	return true;
}