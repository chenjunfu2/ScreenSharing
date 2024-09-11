#pragma once

#include "Socket.hpp"
#include <stdio.h>
#include <stdint.h>

#define DATA_MAX INT_MAX

#pragma pack(push,1)
struct Pack_Head
{
	uint32_t data_size;
	uint32_t flags;
};
#pragma pack(pop)


inline bool MySend(SOCKET s, const char *buf, int len)
{
reSend:
	int ret = send(s, buf, len, 0);
	if (ret <= 0 && len != 0)
	{
		int err = WSAGetLastError();
		printf("send error[%d],lasterror[%d]\n", ret, err);
		return false;
	}
	else if (ret < len)
	{
		buf += ret;
		len -= ret;
		goto reSend;
	}

	return true;
}

inline bool MyRecv(SOCKET s, char *buf, int len)
{
reRecv:
	int ret = recv(s, buf, len, 0);
	if (ret <= 0 && len != 0)
	{
		int err = WSAGetLastError();
		printf("recv error[%d],lasterror[%d]\n", ret, err);
		return false;
	}
	else if (ret < len)
	{
		buf += ret;
		len -= ret;
		goto reRecv;
	}

	return true;
}


int Send_Screen(SOCKET &socket, BITMAPINFO *pBitmap, DWORD dwBitmapInfoSize)
{
	Pack_Head ph;
	uint8_t *pData = (uint8_t *)pBitmap;

	//发送开始封包
	ph.data_size = sizeof(dwBitmapInfoSize);
	ph.flags = 0;//封包开始
	//发送包头和数据
	MySend(socket, (char *)&ph, sizeof(Pack_Head));
	MySend(socket, (char *)&dwBitmapInfoSize, ph.data_size);

	DWORD dwIndex = 0;
	bool bLoop = true;
	while (bLoop)
	{
		if (dwBitmapInfoSize - dwIndex > DATA_MAX)
		{
			ph.flags = 1;//数据发送
			ph.data_size = DATA_MAX;
		}
		else
		{
			ph.flags = -1;//数据结束
			ph.data_size = (dwBitmapInfoSize - dwIndex);
			bLoop = false;
		}

		//发送包头和数据
		MySend(socket, (char *)&ph, sizeof(Pack_Head));
		MySend(socket, (char *)&pData[dwIndex], ph.data_size);//data_size允许为0，send会发送0长度数据报
		dwIndex += ph.data_size;
	}

	return 0;
}

int Recv_Screen(SOCKET &socket, BITMAPINFO *&pBitmap)
{
	Pack_Head ph;
	DWORD dwBitmapInfoSize;

	//接收一个封包
	MyRecv(socket, (char *)&ph, sizeof(Pack_Head));
	if (ph.flags == 0)
	{
		MyRecv(socket, (char *)&dwBitmapInfoSize, ph.data_size);
		pBitmap = (BITMAPINFO *)malloc(dwBitmapInfoSize);
	}
	else
	{
		printf("error!\n");
		return -1;
	}

	uint8_t *pData = (uint8_t *)pBitmap;
	DWORD dwIndex = 0;
	do
	{
		//接收一个封包
		MyRecv(socket, (char *)&ph, sizeof(Pack_Head));

		if (ph.flags != 1 && ph.flags != -1)
		{
			free(pBitmap);
			printf("error!\n");
			return -1;
		}

		if (ph.data_size > DATA_MAX)
		{
			free(pBitmap);
			printf("error!\n");
			return -1;
		}

		if (dwIndex + ph.data_size > dwBitmapInfoSize)//防止越界
		{
			free(pBitmap);
			printf("error!\n");
			return -1;
		}

		//接受数据
		MyRecv(socket, (char *)&pData[dwIndex], ph.data_size);
		dwIndex += ph.data_size;

	} while (ph.flags != -1);

	return 0;
}