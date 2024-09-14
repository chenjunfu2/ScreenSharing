#include "Client.hpp"
#include "Server.hpp"
#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>
#pragma comment(lib, "winmm.lib")//timeSetEvent函数使用，请务必包含

#include <stdio.h>

#define FPS 25

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
	Thread_Queue<SendInfo, 2> tQueue;//最多缓存2帧
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

	//释放桌面DC
	ReleaseDC(NULL, dcScreen);
}

int ClientMain(SOCKET &socket)
{
	SendThreadParam param;
	param.bLoop = true;
	param.socket = socket;
	param.hEvent = CreateEventW(NULL, false, false, NULL);//自动复位，默认未开启

	//HANDLE hThread = CreateThread(NULL, 0, SendThreadProc, &param, 0, NULL);
	//CloseHandle(hThread);//不使用该句柄，关闭以避免内存泄漏

	//设置截图定时循环，该循环会在新的线程中运行
	timeSetEvent(1000 / FPS, 0, CaptureScreen, (DWORD_PTR)&param, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);

	//不创建新线程了，把当前线程利用起来
	return SendThreadProc((LPVOID)&param);

	//无限挂起
	//Sleep(INFINITE);
	return 0;
}


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
