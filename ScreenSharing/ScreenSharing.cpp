#include "Client.hpp"
#include "Server.hpp"
#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>
#pragma comment(lib, "winmm.lib")//timeSetEvent函数使用，请务必包含

#include <stdio.h>

#define FPS 60

int ClientMain(SOCKET &socket);
int ServerMain(SOCKET &socket);

int CaptureAnImage(HWND hWnd);

int main(void)
{	
	printf("C/S? :");
	int choose;
	choose = getchar();

	SOCKET socket;
	bool is_client;

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
		is_client = true;
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
		is_client = false;
	}
	else
	{
		printf("error!\n");
		return -1;
	}

	//链接成功

	int ret = -1;

	if (is_client)
	{
		ret = ClientMain(socket);
	}
	else
	{
		ret = ServerMain(socket);
	}


	Sleep(INFINITE);

	return ret;
}

void CALLBACK CaptureScreen(UINT uTimerID, UINT Reserved0, DWORD_PTR dwUser, DWORD_PTR Reserved1, DWORD_PTR Reserved2)
{
	HWND hwDesktop = GetDesktopWindow();
	HDC dcDesktop = GetDC(hwDesktop);

	//创建兼容DC
	HDC dcComp = CreateCompatibleDC(dcDesktop);
	//获取兼容DC宽高
	int iCompWidt = GetDeviceCaps(dcComp, HORZRES);
	int iCompHigh = GetDeviceCaps(dcComp, VERTRES);

	//创建兼容位图
	HBITMAP bmpComp = CreateCompatibleBitmap(dcDesktop, iCompWidt, iCompHigh);
	//位图选入DC
	SelectObject(dcComp, bmpComp);

	//截图到兼容DC内
	BitBlt(dcComp, 0, 0, iCompWidt, iCompHigh, dcDesktop, 0, 0, SRCCOPY);


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
	GetDIBits(dcDesktop, bmpComp, 0, bitmap.bmHeight, &pData[sizeof(BitmapHeader) + dwPaletteSize], pBitmapInfo, DIB_RGB_COLORS);

	//发送DIB
	Send_Screen(*(SOCKET *)dwUser, pBitmapInfo, dwBitmapInfoSize);

	//释放
	free(pBitmapInfo);

	//删除DDB位图
	DeleteObject(bmpComp);
	//删除兼容DC
	DeleteObject(dcComp);

	//释放桌面DC
	ReleaseDC(hwDesktop, dcDesktop);
}

int ClientMain(SOCKET &socket)
{
	timeSetEvent(1000 / FPS, 0, CaptureScreen, (DWORD_PTR)&socket, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
	Sleep(INFINITE);
	return 0;
}

struct ThreadParam
{
	bool bLoop;
	SOCKET socket;
	HWND hWnd;
	Thread_Queue<BITMAPINFO *, 4> tQueue;//最多缓存4帧
};


DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	ThreadParam &param = *(ThreadParam *)lpParameter;
	BITMAPINFO *pBitmapInfo;

	while (param.bLoop)
	{
		if (Recv_Screen(param.socket, pBitmapInfo) != 0)
		{
			continue;
		}

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
	static BITMAPINFO *pBitmapInfo;
	static ThreadParam param;
	static HANDLE hThread;
	switch (message)
	{
	case WM_CREATE:
		{
			//接收屏幕位图
			param.bLoop = true;
			param.socket = *(SOCKET *)((CREATESTRUCT *)lParam)->lpCreateParams;
			param.hWnd = hWnd;
			Recv_Screen(param.socket, pBitmapInfo);

			hThread = CreateThread(NULL, 0, ThreadProc, &param, 0, NULL);
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

			//获取窗口宽高
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

int ServerMain(SOCKET &socket)
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