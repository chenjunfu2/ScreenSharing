#pragma once

#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>
#pragma comment(lib, "winmm.lib")//timeSetEvent函数使用，请务必包含

#define FPS 25

struct SendInfo
{
	BITMAPINFO *pBitmap;
	DWORD dwBitmapInfoSize;
};

struct SendThreadParam
{
	bool bLoop;
	SOCKET socket;
	HANDLE hEvent;
	Thread_Queue<SendInfo, 4> tQueue;//最多缓存4帧
};


DWORD WINAPI SendThreadProc(LPVOID lpParameter)
{
	SendThreadParam &param = *(SendThreadParam *)lpParameter;
	SendInfo info;

	while (param.bLoop)
	{
		if (!param.tQueue.pop(info))//队列为空则无限等待事件对象直到触发
		{
			WaitForSingleObject(param.hEvent, INFINITE);
			continue;
		}

		//不管有没有成功都按成功来算
		Send_Screen(param.socket, info.pBitmap, info.dwBitmapInfoSize);
		//发送完毕释放内存
		free(info.pBitmap);
	}

	return 0;
}


void CALLBACK CaptureScreen(UINT uTimerID, UINT Reserved0, DWORD_PTR dwUser, DWORD_PTR Reserved1, DWORD_PTR Reserved2)
{
	HDC dcScreen = GetDC(NULL);//获取屏幕DC
	//获取屏幕宽高
	int iScreenWidt = GetDeviceCaps(dcScreen, DESKTOPHORZRES);
	int iScreenHigh = GetDeviceCaps(dcScreen, DESKTOPVERTRES);

	//创建兼容DC
	HDC dcComp = CreateCompatibleDC(dcScreen);

	//创建兼容位图
	HBITMAP bmpComp = CreateCompatibleBitmap(dcScreen, iScreenWidt, iScreenHigh);
	//位图选入DC
	SelectObject(dcComp, bmpComp);

	//截图到兼容DC内
	BitBlt(dcComp, 0, 0, iScreenWidt, iScreenHigh, dcScreen, 0, 0, SRCCOPY);


	//获取位图信息
	BITMAP bitmap;
	GetObjectW(bmpComp, sizeof(BITMAP), &bitmap);


	//DDB设备相关位图转换到DIB设备无关位图
	BITMAPINFOHEADER BitmapHeader =
	{
		.biSize = sizeof(BITMAPINFOHEADER),
		.biWidth = bitmap.bmWidth,
		.biHeight = bitmap.bmHeight,
		.biPlanes = 1,
		.biBitCount = bitmap.bmBitsPixel,
		.biCompression = BI_RGB,
		//剩下的都是0，直接不写了，默认初始化为0
	};


	DWORD dwPaletteSize = (bitmap.bmBitsPixel <= 8) ? (1 << bitmap.bmBitsPixel) * sizeof(RGBQUAD) : 0;
	DWORD dwBitSize = bitmap.bmWidthBytes * bitmap.bmHeight;
	DWORD dwBitmapInfoSize = sizeof(BitmapHeader) + dwPaletteSize + dwBitSize;
	BITMAPINFO *pBitmapInfo = (BITMAPINFO *)malloc(dwBitmapInfoSize);
	//拷贝bmp头
	memcpy(pBitmapInfo, &BitmapHeader, sizeof(BitmapHeader));

	//转换到DIB设备无关位图
	uint8_t *pData = (uint8_t *)pBitmapInfo;
	GetDIBits(dcScreen, bmpComp, 0, bitmap.bmHeight, &pData[sizeof(BitmapHeader) + dwPaletteSize], pBitmapInfo, DIB_RGB_COLORS);

	//发送DIB
	SendThreadParam &param = *(SendThreadParam *)dwUser;
	if (!param.tQueue.push(SendInfo{ pBitmapInfo, dwBitmapInfoSize }))
	{
		//压入失败，缓存已满，直接丢弃
		free(pBitmapInfo);
	}

	//成功，触发事件对象，通知线程退出等待开始发送数据
	SetEvent(param.hEvent);

	//删除DDB位图
	DeleteObject(bmpComp);
	//删除兼容DC
	DeleteObject(dcComp);

	//释放屏幕DC
	ReleaseDC(NULL, dcScreen);
}

int Share(SOCKET &socket)
{
	SendThreadParam param;
	param.bLoop = true;
	param.socket = socket;
	param.hEvent = CreateEventW(NULL, false, false, NULL);//自动复位，默认未开启

	//HANDLE hThread = CreateThread(NULL, 0, SendThreadProc, &param, 0, NULL);
	//CloseHandle(hThread);//不使用该句柄，关闭以避免句柄泄漏

	//设置截图定时循环，该循环会在新的线程中运行
	timeSetEvent(1000 / FPS, 0, CaptureScreen, (DWORD_PTR)&param, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);

	//不创建新线程了，把当前线程利用起来
	return SendThreadProc((LPVOID)&param);

	//无限挂起
	//Sleep(INFINITE);
	return 0;
}