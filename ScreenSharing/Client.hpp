#pragma once

#include "Socket.hpp"
#include <stdio.h>

bool Client(SOCKET& sockClient, char* szIPAddress, int& szIPAnum)
{
	WSADATA wd;
	if (WSAStartup(MAKEWORD(2, 2), &wd))
	{
		printf("�����׽��ֿ�ʧ��.\n");
		return false;
	}

	if (LOBYTE(wd.wVersion) != 2 || HIBYTE(wd.wVersion) != 2)
	{
		printf("���ص��׽��ֿ�汾��һ��.\n");
		return false;
	}

	sockClient = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr = inet_addr(szIPAddress);
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(szIPAnum);

	//���ӷ�����
	if (SOCKET_ERROR == connect(sockClient, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)))
	{
		printf("���ӷ�����ʧ��.\n");
		return false;
	}

	printf("��ϲ��.���ӷ������ɹ�.\n");

	return true;
}