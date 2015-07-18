/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#include "vld.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#pragma comment(lib, "Msimg32.lib")	//for TransparentBlt

extern "C"
{
#include "../cGIF/cGIF.h"
}
#pragma comment(lib, "../Debug/cGIF.lib")

#define APP_TITLE "GIF Viewer via GDI (Static)"

//������ɫ��RGB˳��ע��GDI��RGBTRIPLEΪBGR˳��
typedef struct _tagRGBTriple
{
	BYTE r;
	BYTE g;
	BYTE b;
} RGBTriple;

// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("ViewViaGDI_StaticWindowClass");
// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T(APP_TITLE);

//��ͼ��ر���
HPALETTE hpal = 0;//��ɫ����
HBITMAP bitmap = 0;//ͼ����
HDC hdcsource = 0;
BITMAPINFO* bmi = nullptr;
RECT canvasRect = { 0 };
UINT backgroundColor;
UINT transparentColor;
HBRUSH backgroundBrush = nullptr;

//cGIF����
unsigned int canvasWidth = 0, canvasHeight = 0;
unsigned int image_position_x = 0, image_position_y = 0;
unsigned int imageWidth = 0;
unsigned int imageHeight = 0;
RGBTriple* palette = nullptr;
unsigned char palette_color_count;
unsigned char background_color_index;
unsigned char transparent_color_index;
char flag = 0;
int disposalMethod = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_BTNSHADOW + 1);// (HBRUSH)GetStockObject(WHITE_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, _T("RegisterClassExʧ�ܣ�"), _T(APP_TITLE), NULL);
		return 1;
	}

	HWND hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		600, 600,
		NULL,
		NULL,
		hInstance,
		NULL
		);
	if (!hWnd)
	{
		MessageBox(NULL, _T("CreateWindowʧ�ܣ�"), _T(APP_TITLE), NULL);
		return 1;
	}

	unsigned char* pColorIndexArray = NULL;
	cGif_Error error = cGif_decode_static_indexed(
		lpCmdLine,
		&pColorIndexArray,
		&canvasWidth, &canvasHeight,
		&background_color_index,
		&image_position_x, &image_position_y,
		&imageWidth, &imageHeight,
		(unsigned char**)&palette, &palette_color_count,
		&transparent_color_index,
		&flag);
	if (error != cGif_Error::cGif_Success)
	{
		char buffer[128];
		sprintf(buffer, "����GIFͼ��ʧ�ܣ�����%s", cGif_error_text(error));
		MessageBoxA(NULL, buffer, APP_TITLE, NULL);
		return 1;
	}
	canvasRect.left = 150;
	canvasRect.top = 150;
	canvasRect.right = canvasRect.left + canvasWidth;
	canvasRect.bottom = canvasRect.top + canvasHeight;
	
	//������������
	//һ��ͼ�����ݵĴ�С������4�ֽڵı����������Ҫ��0����
	int lineByteSize =
		(imageWidth%4 == 0) ? imageWidth : (imageWidth + (4 - imageWidth % 4));
	unsigned char* vColorIndex = (unsigned char*)malloc(imageHeight*lineByteSize);
	memset(vColorIndex, 0xff, imageHeight*lineByteSize);
	for (unsigned int i = 0; i < imageHeight; ++i)
	{
		for (unsigned int j = 0; j < imageWidth; ++j)
		{
			*(vColorIndex + lineByteSize*i + j) = *(pColorIndexArray + imageWidth*i + j);
		}
	}
	//The color index data have been copied, so release it. | ��ɫ�������Ѿ����ƹ��ˣ�����û���ˣ��ͷ���
	free(pColorIndexArray);

	//������ɫ
	if (flag & 0x01)
	{
		backgroundColor = RGB(palette[background_color_index].r,
			palette[background_color_index].g, palette[background_color_index].b);
	}
	else
	{
		backgroundColor = RGB(0xff,0xff,0xff);//Default background color is white
	}

	//����͸��ɫ
	if (flag & 0x02)
	{
		UINT transparentColorOrginal = RGB(palette[transparent_color_index].r,
			palette[transparent_color_index].g, palette[transparent_color_index].b);
		/*
		ע�⣺
		GIF����ɫ���У�͸��ɫ���ܱ�����Ϊ���е���ͨ��ɫ��������ɫ���е�3����ɫ�Ǻ�ɫ����178����ɫҲ�Ǻ�ɫ��
		����178������Ϊ��͸��ɫ��������BMP�޷����������ķ�ʽʵ��͸��ɫ��������Ҫ�滻��178����ɫΪ��ɫ��
		��û�г��ֹ�����ɫ��Ȼ����TransparentBlt��ָ������ɫΪ͸��ɫ��������Ⱦ��
		ע�⣺��Ⱦ��̬ͼ��ʱ��������ô������Ϊ֮ǰ��֡�����ĸı��Ӱ������֡��
		*/
		RGBTriple& transparentColorTmp = palette[transparent_color_index];
		{
			bool bTransparentColorAlreadyUsed = false;
			//����GIF��ɫ���ж�͸��ɫ�Ƿ������е���ͨ��ɫ
			for (int i = 0; i <= palette_color_count; ++i)
			{
				if (transparentColorTmp.r == palette[i].r
					&& transparentColorTmp.g == palette[i].g
					&& transparentColorTmp.b == palette[i].b)
				{
					bTransparentColorAlreadyUsed = true;
					break;
				}
			}
			//ȷ��͸��ɫ�����е���ͨ��ɫ�Ļ�����һ����ɫ����û�е���ɫ�����
			if (bTransparentColorAlreadyUsed == true)
			{
				//�����������ɫ����Ϊ256ɫ��0xffffffɫ�л��Ǻ��ٵ�
				srand((int)palette);//����Ҹ�Runtimeֵ�����ӣ������time()���üӿ�
				BYTE r, g, b;
				while (true)
				{
					r = BYTE(rand() % 256);
					g = BYTE(rand() % 256);
					b = BYTE(rand() % 256);
					int j = 0;
					for (j = 0; j <= palette_color_count; ++j)
					{
						if (RGB(palette[j].r, palette[j].g, palette[j].b) == RGB(r, g, b))
						{
							break;
						}
					}
					if (j == palette_color_count+1)
						break;
				}
				transparentColorTmp.r = r;
				transparentColorTmp.g = g;
				transparentColorTmp.b = b;
				//Replace original transparent color | �滻ԭ����͸��ɫ
				palette[transparent_color_index] = transparentColorTmp;
				//Set transparent color for drawing bmp | ���õ�ǰ��͸��ɫ���ڻ���BMP
				transparentColor = RGB(transparentColorTmp.r,
					transparentColorTmp.g, transparentColorTmp.b);
			}
			else
			{
				transparentColor = transparentColorOrginal;
			}
		}
	}

	//��ȡdisposal method
	int disposalMethod = ((flag & 0x1C) >> 2);

#if 1
	//��������ɫ��ˢ
	backgroundBrush = CreateSolidBrush(backgroundColor);


	//����������ˢ
#else
	//���ھ�̬ͼ��
	//���disposal method��2��3��
	//	�����͸��ɫ����ʹ��͸��ɫ��Ϊcanvas����������ʹ�ñ���ɫ��
	//���disposal method����2��3
	//  ���ù�canvas������Ҳ����˵���û���canvas����
	if (disposalMethod == 2 || disposalMethod == 3)
	{
		if (flag & 0x01)//�����͸��ɫ
			backgroundBrush = wcex.hbrBackground;
		else
			backgroundBrush = CreateSolidBrush(backgroundColor);
	}
#endif
	//����������Bitmap��Ϣ
	bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD));//��256ɫ����bmpInfoHeader.biBitCount��ָ��ͬ
	//ͷ��Ϣ
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);   //BITMAPINFOHEADER�Ĵ�С
	bmi->bmiHeader.biWidth = imageWidth;				//ͼ���ȣ�������Ϊ��λ��
	bmi->bmiHeader.biHeight = -int(imageHeight);		//ͼ��߶ȣ�������Ϊ��λ����ע�⣡�˴�ȡ��ֵ����ΪλͼĬ�ϵ�һ����ͼ��������һ�У���cGif_decode�õ������ݵĵ�һ����ͼ��������һ��
	bmi->bmiHeader.biPlanes = 1;						//���뱻����Ϊ1
	bmi->bmiHeader.biBitCount = 8;						//ͳһΪ256ɫͼ
	bmi->bmiHeader.biCompression = BI_RGB;				//��ѹ����һ��bmp��ʽ
	bmi->bmiHeader.biSizeImage = 0;						//biCompressionΪBI_RGBʱָ����ֵΪ0
	//���迼�ǵ�ֵ
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;
	//��ɫ��
	//In fact, palette_color_count+1 is the number of colors in the palette
	//But type of palette_color_count is unsigned char, whose max value is 255,
	// so 0 is use to represent 1 color, 2 is use to represent 2 colors, etc.
	for (int i = 0; i <= palette_color_count; ++i)
	{
		bmi->bmiColors[i].rgbRed = palette[i].r;
		bmi->bmiColors[i].rgbGreen = palette[i].g;
		bmi->bmiColors[i].rgbBlue = palette[i].b;
		bmi->bmiColors[i].rgbReserved = 0;
	}
	free(palette);

	//����bmp
	hdcsource = CreateCompatibleDC(NULL);
	if (NULL == hdcsource)
	{
		MessageBox(NULL, _T("CreateCompatibleDCʧ�ܣ�"), _T(APP_TITLE), NULL);
		return 1;
	}
	bitmap = CreateDIBitmap(GetDC(hWnd), &bmi->bmiHeader, CBM_INIT, vColorIndex, bmi, DIB_RGB_COLORS);
	free(vColorIndex);
	if (NULL == bitmap)
	{
		MessageBox(NULL, _T("CreateDIBitmapʧ�ܣ�"), _T(APP_TITLE), NULL);
		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	//Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		{
			HGDIOBJ original = SelectObject(hdcsource, bitmap);
			if (NULL == original || HGDI_ERROR == original)
			{
				MessageBox(NULL, _T("SelectObjectʧ�ܣ�"), _T(APP_TITLE), NULL);
				return 1;
			}

			//���ƻ���
			//Draw a background soild rect for the canvas | ��һ�����ο򱳾���ʾ����
			if (0 == FillRect(hdc, &canvasRect, backgroundBrush))
			{
				OutputDebugString(_T("FillRectʧ�ܣ�\n"));
				return E_FAIL;
			}

			//Draw the image | ����ͼ��
			if (! (flag & 0x02))//No transparent color | û��͸��ɫ
			{
				if (NULL == BitBlt(hdc,
					canvasRect.left + image_position_x, canvasRect.top + image_position_y,
					imageWidth, imageHeight,
					hdcsource, 0, 0, SRCCOPY))
				{
					OutputDebugString(_T("BitBltʧ�ܣ�\n"));
					return E_FAIL;
				}
			}
			else
			{
				if (FALSE == TransparentBlt(
					hdc,
					canvasRect.left + image_position_x, canvasRect.top + image_position_y,
					imageWidth, imageHeight,
					hdcsource, 0, 0, imageWidth, imageHeight,
					transparentColor))
				{
					OutputDebugString(_T("TransparentBltʧ�ܣ�\n"));
					return E_FAIL;
				}
			}

			SelectObject(hdc, original);
		}
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		DeleteDC(hdcsource);
		DeleteObject(bitmap);
		DeleteObject(backgroundBrush);
		free(bmi);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}