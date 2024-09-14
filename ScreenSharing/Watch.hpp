#pragma once

#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>

void SaveToFile(const BITMAPINFO *pBitmap, DWORD dwBitmapInfoSize)
{
	//���潨��ʹ�÷������ص�IO
	//overlapped IO
	return;
}


struct RecvThreadParam
{
	bool bLoop;
	SOCKET socket;
	HWND hWnd;
	Thread_Queue<BITMAPINFO *, 4> tQueue;//��໺��4֡
};


DWORD WINAPI RecvThreadProc(LPVOID lpParameter)
{
	RecvThreadParam &param = *(RecvThreadParam *)lpParameter;
	BITMAPINFO *pBitmapInfo;

	while (param.bLoop)
	{
		DWORD dwBitmapInfoSize;
		if (Recv_Screen(param.socket, pBitmapInfo, &dwBitmapInfoSize) != 0)//�õ���Ϊ�������ã�ֱ����ȫ�ɹ���ʧ�ܲŷ���
		{
			continue;//ʧ������
		}

		//���浽����
		//SaveToFile(pBitmapInfo, dwBitmapInfoSize);

		//�������
		if (!param.tQueue.push(std::move(pBitmapInfo)))
		{
			//���ˣ�ֱ�Ӷ���
			free(pBitmapInfo);
		}
		//��Ч���ͻ�����֪ͨ����л���
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
			//������Ļλͼ
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

			if (!param.tQueue.Empty())//������зǿ��򵯳�����֡
			{
				BITMAPINFO *old = pBitmapInfo;
				if (param.tQueue.pop(pBitmapInfo))//�����ɹ����ͷ�ԭ���ģ����򲻱������ͷ�
				{
					free(old);
				}
			}

			if (pBitmapInfo == NULL)//ΪNULL˵����һ֡��δ���յ�
			{
				//ֱ�ӽ������Ʒ���
				EndPaint(hWnd, &ps);
				break;
			}


			//��������DC
			HDC dcComp = CreateCompatibleDC(hdc);

			//��ȡbitmap���
			int iCompWidt = pBitmapInfo->bmiHeader.biWidth;
			int iCompHigh = pBitmapInfo->bmiHeader.biHeight;

			//��������λͼ
			HBITMAP bmpComp = CreateCompatibleBitmap(hdc, iCompWidt, iCompHigh);
			//λͼѡ��DC
			SelectObject(dcComp, bmpComp);

			DWORD dwPaletteSize = (pBitmapInfo->bmiHeader.biBitCount <= 8) ? (1 << pBitmapInfo->bmiHeader.biBitCount) * sizeof(RGBQUAD) : 0;

			//ת�����豸���λͼDDB
			uint8_t *pData = (uint8_t *)pBitmapInfo;
			SetDIBits(hdc, bmpComp, 0, pBitmapInfo->bmiHeader.biHeight, &pData[sizeof(pBitmapInfo->bmiHeader) + dwPaletteSize], pBitmapInfo, DIB_RGB_COLORS);

			//��ȡ���ڿͻ������
			RECT ClientRect;
			GetClientRect(hWnd, &ClientRect);
			int iClientWidt = ClientRect.right - ClientRect.left;
			int iClientHigh = ClientRect.bottom - ClientRect.top;
			//���������Ų���Ӧ����
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

			//���ŵ����ڴ�С����ͼ
			SetStretchBltMode(hdc, HALFTONE);
			StretchBlt(hdc, iOffX, iOffY, iZoomW, iZoomH, dcComp, 0, 0, iCompWidt, iCompHigh, SRCCOPY);

			//ɾ��bmp��dc
			DeleteObject(bmpComp);
			DeleteObject(dcComp);

			//��������
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		//�ͷ��߳�
		param.bLoop = false;
		WaitForSingleObject(hThread, INFINITE);//�ȴ��˳�
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int Watch(SOCKET &socket)
{
	//��������
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

	// ִ��Ӧ�ó����ʼ��:

	HWND hWnd = CreateWindowW(L"ScreenShow", L"ScreenShow", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, nullptr, (LPVOID)&socket);

	if (!hWnd)
	{
		return -1;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;

	// ����Ϣѭ��:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return 0;
}