#pragma once

#include "Send_Recv_Screen.hpp"
#include "Thread_Queue.hpp"

#include <Windows.h>
#pragma comment(lib, "winmm.lib")//timeSetEvent����ʹ�ã�����ذ���

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
	Thread_Queue<SendInfo, 4> tQueue;//��໺��4֡
};


DWORD WINAPI SendThreadProc(LPVOID lpParameter)
{
	SendThreadParam &param = *(SendThreadParam *)lpParameter;
	SendInfo info;

	while (param.bLoop)
	{
		if (!param.tQueue.pop(info))//����Ϊ�������޵ȴ��¼�����ֱ������
		{
			WaitForSingleObject(param.hEvent, INFINITE);
			continue;
		}

		//������û�гɹ������ɹ�����
		Send_Screen(param.socket, info.pBitmap, info.dwBitmapInfoSize);
		//��������ͷ��ڴ�
		free(info.pBitmap);
	}

	return 0;
}


void CALLBACK CaptureScreen(UINT uTimerID, UINT Reserved0, DWORD_PTR dwUser, DWORD_PTR Reserved1, DWORD_PTR Reserved2)
{
	HDC dcScreen = GetDC(NULL);//��ȡ��ĻDC
	//��ȡ��Ļ���
	int iScreenWidt = GetDeviceCaps(dcScreen, DESKTOPHORZRES);
	int iScreenHigh = GetDeviceCaps(dcScreen, DESKTOPVERTRES);

	//��������DC
	HDC dcComp = CreateCompatibleDC(dcScreen);

	//��������λͼ
	HBITMAP bmpComp = CreateCompatibleBitmap(dcScreen, iScreenWidt, iScreenHigh);
	//λͼѡ��DC
	SelectObject(dcComp, bmpComp);

	//��ͼ������DC��
	BitBlt(dcComp, 0, 0, iScreenWidt, iScreenHigh, dcScreen, 0, 0, SRCCOPY);


	//��ȡλͼ��Ϣ
	BITMAP bitmap;
	GetObjectW(bmpComp, sizeof(BITMAP), &bitmap);


	//DDB�豸���λͼת����DIB�豸�޹�λͼ
	BITMAPINFOHEADER BitmapHeader =
	{
		.biSize = sizeof(BITMAPINFOHEADER),
		.biWidth = bitmap.bmWidth,
		.biHeight = bitmap.bmHeight,
		.biPlanes = 1,
		.biBitCount = bitmap.bmBitsPixel,
		.biCompression = BI_RGB,
		//ʣ�µĶ���0��ֱ�Ӳ�д�ˣ�Ĭ�ϳ�ʼ��Ϊ0
	};


	DWORD dwPaletteSize = (bitmap.bmBitsPixel <= 8) ? (1 << bitmap.bmBitsPixel) * sizeof(RGBQUAD) : 0;
	DWORD dwBitSize = bitmap.bmWidthBytes * bitmap.bmHeight;
	DWORD dwBitmapInfoSize = sizeof(BitmapHeader) + dwPaletteSize + dwBitSize;
	BITMAPINFO *pBitmapInfo = (BITMAPINFO *)malloc(dwBitmapInfoSize);
	//����bmpͷ
	memcpy(pBitmapInfo, &BitmapHeader, sizeof(BitmapHeader));

	//ת����DIB�豸�޹�λͼ
	uint8_t *pData = (uint8_t *)pBitmapInfo;
	GetDIBits(dcScreen, bmpComp, 0, bitmap.bmHeight, &pData[sizeof(BitmapHeader) + dwPaletteSize], pBitmapInfo, DIB_RGB_COLORS);

	//����DIB
	SendThreadParam &param = *(SendThreadParam *)dwUser;
	if (!param.tQueue.push(SendInfo{ pBitmapInfo, dwBitmapInfoSize }))
	{
		//ѹ��ʧ�ܣ�����������ֱ�Ӷ���
		free(pBitmapInfo);
	}

	//�ɹ��������¼�����֪ͨ�߳��˳��ȴ���ʼ��������
	SetEvent(param.hEvent);

	//ɾ��DDBλͼ
	DeleteObject(bmpComp);
	//ɾ������DC
	DeleteObject(dcComp);

	//�ͷ���ĻDC
	ReleaseDC(NULL, dcScreen);
}

int Share(SOCKET &socket)
{
	SendThreadParam param;
	param.bLoop = true;
	param.socket = socket;
	param.hEvent = CreateEventW(NULL, false, false, NULL);//�Զ���λ��Ĭ��δ����

	//HANDLE hThread = CreateThread(NULL, 0, SendThreadProc, &param, 0, NULL);
	//CloseHandle(hThread);//��ʹ�øþ�����ر��Ա�����й©

	//���ý�ͼ��ʱѭ������ѭ�������µ��߳�������
	timeSetEvent(1000 / FPS, 0, CaptureScreen, (DWORD_PTR)&param, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);

	//���������߳��ˣ��ѵ�ǰ�߳���������
	return SendThreadProc((LPVOID)&param);

	//���޹���
	//Sleep(INFINITE);
	return 0;
}