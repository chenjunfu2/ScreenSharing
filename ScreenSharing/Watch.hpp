#pragma once

#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>

void SaveToFile(const BITMAPINFO *pBitmap, DWORD dwBitmapInfoSize)
{
	//保存建议使用非阻塞重叠IO
	//overlapped IO
	return;
}


struct RecvThreadParam
{
	bool bLoop;
	SOCKET socket;
	HWND hWnd;
	Thread_Queue<BITMAPINFO *, 4> tQueue;//最多缓存4帧
};


DWORD WINAPI RecvThreadProc(LPVOID lpParameter)
{
	RecvThreadParam &param = *(RecvThreadParam *)lpParameter;
	BITMAPINFO *pBitmapInfo;

	while (param.bLoop)
	{
		DWORD dwBitmapInfoSize;
		if (Recv_Screen(param.socket, pBitmapInfo, &dwBitmapInfoSize) != 0)//该调用为阻塞调用，直到完全成功或失败才返回
		{
			continue;//失败重试
		}

		//保存到磁盘
		//SaveToFile(pBitmapInfo, dwBitmapInfoSize);

		//放入队列
		if (!param.tQueue.push(std::move(pBitmapInfo)))
		{
			//满了，直接丢弃
			free(pBitmapInfo);
		}
		//无效化客户区，通知其进行绘制
		InvalidateRect(param.hWnd, NULL, false);
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static BITMAPINFO *pBitmapInfo = NULL;
	static RecvThreadParam param;
	static HANDLE hThread;
	switch (message)
	{
	case WM_CREATE:
		{
			//接收屏幕位图
			param.bLoop = true;
			param.socket = *(SOCKET *)((CREATESTRUCT *)lParam)->lpCreateParams;
			param.hWnd = hWnd;

			hThread = CreateThread(NULL, 0, RecvThreadProc, &param, 0, NULL);
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			if (!param.tQueue.Empty())//如果队列非空则弹出最新帧
			{
				BITMAPINFO *old = pBitmapInfo;
				if (param.tQueue.pop(pBitmapInfo))//弹出成功则释放原来的，否则不变无需释放
				{
					free(old);
				}
			}

			if (pBitmapInfo == NULL)//为NULL说明第一帧还未接收到
			{
				//直接结束绘制返回
				EndPaint(hWnd, &ps);
				break;
			}


			//创建兼容DC
			HDC dcComp = CreateCompatibleDC(hdc);

			//获取bitmap宽高
			int iCompWidt = pBitmapInfo->bmiHeader.biWidth;
			int iCompHigh = pBitmapInfo->bmiHeader.biHeight;

			//创建兼容位图
			HBITMAP bmpComp = CreateCompatibleBitmap(hdc, iCompWidt, iCompHigh);
			//位图选入DC
			SelectObject(dcComp, bmpComp);

			DWORD dwPaletteSize = (pBitmapInfo->bmiHeader.biBitCount <= 8) ? (1 << pBitmapInfo->bmiHeader.biBitCount) * sizeof(RGBQUAD) : 0;

			//转换到设备相关位图DDB
			uint8_t *pData = (uint8_t *)pBitmapInfo;
			SetDIBits(hdc, bmpComp, 0, pBitmapInfo->bmiHeader.biHeight, &pData[sizeof(pBitmapInfo->bmiHeader) + dwPaletteSize], pBitmapInfo, DIB_RGB_COLORS);

			//获取窗口客户区宽高
			RECT ClientRect;
			GetClientRect(hWnd, &ClientRect);
			int iClientWidt = ClientRect.right - ClientRect.left;
			int iClientHigh = ClientRect.bottom - ClientRect.top;
			//按比例缩放并适应窗口
			double dZoomRatioW = (double)iClientWidt / (double)iCompWidt;
			double dZoomRatioH = (double)iClientHigh / (double)iCompHigh;

			int iOffX = 0;
			int iOffY = 0;

			int iZoomW = (double)iCompWidt * dZoomRatioH;
			int iZoomH = (double)iCompHigh * dZoomRatioW;

			if (iZoomW > iClientWidt)
			{
				iZoomW = iClientWidt;
				iOffY = (iClientHigh - iZoomH) / 2;
			}
			else if (iZoomH > iClientHigh)
			{
				iZoomH = iClientHigh;
				iOffX = (iClientWidt - iZoomW) / 2;
			}

			//缩放到窗口大小并绘图
			SetStretchBltMode(hdc, HALFTONE);
			StretchBlt(hdc, iOffX, iOffY, iZoomW, iZoomH, dcComp, 0, 0, iCompWidt, iCompHigh, SRCCOPY);

			//删除bmp和dc
			DeleteObject(bmpComp);
			DeleteObject(dcComp);

			//结束绘制
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		//释放线程
		param.bLoop = false;
		WaitForSingleObject(hThread, INFINITE);//等待退出
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int Watch(SOCKET &socket)
{
	//创建窗口
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = NULL;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"ScreenShow";
	wcex.hIconSm = NULL;

	RegisterClassExW(&wcex);

	// 执行应用程序初始化:

	HWND hWnd = CreateWindowW(L"ScreenShow", L"ScreenShow", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, nullptr, (LPVOID)&socket);

	if (!hWnd)
	{
		return -1;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return 0;
}