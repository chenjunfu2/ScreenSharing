#pragma once

#include "Socket.hpp"
#include <stdio.h>

bool Server(SOCKET& sockServ, int& szIPAnum)
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

	SOCKET sockListen = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���С�� h:host, to: ת�� ��n: net ,  s:short
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(szIPAnum);						//0-��1024�� 1024����
	if (SOCKET_ERROR == bind(sockListen, (sockaddr*)&addrSrv, sizeof(sockaddr)))
	{
		printf("���׽���ʧ��.\n");
		return false;
	}

	if (SOCKET_ERROR == listen(sockListen, 5)) //5p
	{
		printf("����ʧ��,�������磡\n");
		return false;
	}

	printf("���ڼ���...\n");

	SOCKADDR_IN addrClient;
	int nLength = sizeof(SOCKADDR);
	sockServ = accept(sockListen, (SOCKADDR*)&addrClient, &nLength);
	printf("IP:%s�˿�:%d  ��������������\n", inet_ntoa(addrClient.sin_addr), addrClient.sin_port);


	return true;
}