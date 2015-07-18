/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include "vld.h"
#pragma comment(lib, "Msimg32.lib")	//for TransparentBlt

extern "C" 
{
#include "../cGIF/cGIF.h"
}
#pragma comment(lib, "../Debug/cGIF.lib")

#define APP_TITLE "GIF Viewer via GDI (Dynamic)"

//������ɫ��RGB˳��ע��GDI��RGBTRIPLEΪBGR˳��
typedef struct _tagRGBTriple
{
	BYTE r;
	BYTE g;
	BYTE b;
} RGBTriple;

// The main window class name.
static TCHAR szWindowClass[] = _T("ViewViaGDI_DynamicWindowClass");
// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T(APP_TITLE);

//λͼ�����Դ
HPALETTE hpal = 0;//��ɫ����
HBITMAP vBitmap[1024] = { 0 };//ͼ����
HDC hdcsource = 0;
RECT canvasRect = { 0 };
HBRUSH backgroundBrush = nullptr;

//cGIF����
unsigned canvasWidth;
unsigned canvasHeight;
RGBTriple* globalPalette;
unsigned char globalPalette_color_count;
unsigned char background_color_index;
unsigned frameCount;
unsigned* frameTimeInMS;
unsigned char** ppColorIndexArray = NULL;//indexex
unsigned* image_position_x;
unsigned* image_position_y;
unsigned* imageWidth;
unsigned* imageHeight;
RGBTriple** palette;
unsigned char* palette_color_count;
unsigned char* transparent_color_index;
char globalFlag;
char* flag;

UINT backgroundColor;
UINT* transparentColor;
int* disposalMethod;

//�����������
unsigned currentFrame = 0;

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
	wcex.hbrBackground = (HBRUSH)(COLOR_BTNSHADOW + 1);
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

	cGif_Error error = cGif_decode_dynamic_indexed(
		lpCmdLine,
		&canvasWidth, &canvasHeight,
		(unsigned char**)&globalPalette,
		&globalPalette_color_count,
		&background_color_index,
		&globalFlag,
		&frameCount,
		&frameTimeInMS,
		&ppColorIndexArray,
		&image_position_x, &image_position_y,
		&imageWidth, &imageHeight,
		(unsigned char***)&palette, &palette_color_count,
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

	disposalMethod = new int[frameCount];
	transparentColor = new UINT[frameCount];

	/*
	��������Ⱦ��
	1.��ȡGIF�ļ����õ�ÿһ֡��ͼ������
	2.����˳����Ⱦÿһ֡ͼ����Ⱦʱ��ѭDisposal ��ʽ
	*/

	//Canvas��Ϣ
	//��canvasWidth����canvasHeight��
	//ȫ����ɫ��globalPalette��ȫ����ɫ����ɫ����globalPalette_color_count
	//������ɫ��ȫ����ɫ���е�����background_color_index
	//ͼ��֡��frameCount

	//������ɫ������ɫָ����(canvas)����ɫ
	if (globalFlag & 0x01)
	{
		backgroundColor = RGB(globalPalette[background_color_index].r,
			globalPalette[background_color_index].g, globalPalette[background_color_index].b);
	}
	else
	{
		backgroundColor = RGB(0xff, 0xff, 0xff);//Default background color is white
	}
	//��������ɫ��ˢ
	backgroundBrush = CreateSolidBrush(backgroundColor);

	//������Global Color Table���ļ���GCT�ṩ����Щû��Local Color Table��ͼ��֡ʹ�ã�
	//���ͼ��֡����LCT��������Լ���LCT��Ⱦ
	//����ļ���û��GCT����ͼ��֡Ҳû��LCT���򱨴�
	bool HasGCT = (globalPalette != NULL);

	//ÿ֡����1��bitmap����
	for (unsigned int i = 0; i < frameCount; i++)
	{
		HBITMAP& bitmap = vBitmap[i];

		unsigned char* pColorIndexArray = ppColorIndexArray[i];
		RGBTriple* colorTable = nullptr;
		int colorTableColorCount = 0;
		//Select proper palette: prefer local color table than global color table
		//TODO If neither of LCT and GCT exists, fallback to a default color table.
		if (palette == nullptr || palette[i] == nullptr)
		{
			if (globalPalette_color_count == 0)
				colorTableColorCount = 256;
			else
				colorTableColorCount = globalPalette_color_count;

			colorTable = new RGBTriple[colorTableColorCount];
			memcpy(colorTable, globalPalette, colorTableColorCount*sizeof(RGBTriple));
		}
		else
		{
			if (palette_color_count[i] == 0)
				colorTableColorCount = 256;
			else
				colorTableColorCount = palette_color_count[i];

			colorTable = new RGBTriple[colorTableColorCount];
			memcpy(colorTable, palette[i], colorTableColorCount*sizeof(RGBTriple));
		}

		//Disposal ��ʽ
		disposalMethod[i] = ((flag[i] & 0x1C) >> 2);
		
		/*
		ע�⣺����GIFͼ���Ƕ���λ�ģ�֮�������8λ������Ⱦ
		*/

		//����������Bitmap��Ϣ
		BITMAPINFO* bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD));
		//ͷ��Ϣ
		bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);   //BITMAPINFOHEADER�Ĵ�С
		bmi->bmiHeader.biWidth = imageWidth[i];				//ͼ���ȣ�������Ϊ��λ��
		bmi->bmiHeader.biHeight = -int(imageHeight[i]);		//ͼ��߶ȣ�������Ϊ��λ����ע�⣡�˴�ȡ��ֵ����ΪλͼĬ�ϵ�һ����ͼ��������һ�У���cGif_decode�õ������ݵĵ�һ����ͼ��������һ��
		bmi->bmiHeader.biPlanes = 1;						//���뱻����Ϊ1
		bmi->bmiHeader.biBitCount = 8;						//������8λ��Ⱦ
		bmi->bmiHeader.biCompression = BI_RGB;				//��ѹ����һ��bmp��ʽ
		bmi->bmiHeader.biSizeImage = 0;						//biCompressionΪBI_RGBʱָ����ֵΪ0
		//���迼�ǵ�ֵ
		bmi->bmiHeader.biXPelsPerMeter = 0;
		bmi->bmiHeader.biYPelsPerMeter = 0;
		bmi->bmiHeader.biClrUsed = 0;
		bmi->bmiHeader.biClrImportant = 0;

		//͸��ɫ
		if (flag[i] & 0x01)
		{
			/*
			ע�⣺
				GIF����ɫ���У�͸��ɫ���ܱ�����Ϊ���е���ͨ��ɫ��������ɫ���е�3����ɫ�Ǻ�ɫ����178����ɫҲ�Ǻ�ɫ��
				����178������Ϊ��͸��ɫ��������BMP�޷����������ķ�ʽʵ��͸��ɫ��������Ҫ�滻��178����ɫΪ��ɫ��
				��û�г��ֹ�����ɫ��Ȼ����TransparentBlt��ָ������ɫΪ͸��ɫ��������Ⱦ��
				*/
			UINT transparentColorOrginal = RGB(colorTable[transparent_color_index[i]].r,
				colorTable[transparent_color_index[i]].g, colorTable[transparent_color_index[i]].b);
			RGBTriple& transparentColorTmp = colorTable[transparent_color_index[i]];
			{
				bool bTransparentColorAlreadyUsed = false;
				//����GIF��ɫ���ж�͸��ɫ�Ƿ������е���ͨ��ɫ
				for (int j = 0; j < colorTableColorCount; ++j)
				{
					if (transparentColorTmp.r == colorTable[j].r
						&& transparentColorTmp.g == colorTable[j].g
						&& transparentColorTmp.b == colorTable[j].b)
					{
						bTransparentColorAlreadyUsed = true;
						break;
					}
				}
				//ȷ��͸��ɫ�����е���ͨ��ɫ�Ļ�����һ����ɫ����û�е���ɫ�����
				if (bTransparentColorAlreadyUsed == true)
				{
					//�����������ɫ����Ϊ256ɫ��0xffffffɫ�л��Ǻ��ٵ�
					srand((int)colorTable);//����Ҹ�Runtimeֵ�����ӣ������time()���üӿ�
					BYTE r, g, b;
					while (true)
					{
						r = BYTE(rand() % 256);
						g = BYTE(rand() % 256);
						b = BYTE(rand() % 256);
						int j = 0;
						for (j = 0; j < colorTableColorCount; ++j)
						{
							if (RGB(colorTable[j].r, colorTable[j].g, colorTable[j].b) == RGB(r, g, b))
							{
								break;
							}
						}
						if (j == colorTableColorCount)
							break;
					}
					transparentColorTmp.r = r;
					transparentColorTmp.g = g;
					transparentColorTmp.b = b;
					//Replace original transparent color | �滻ԭ����͸��ɫ
					colorTable[transparent_color_index[i]] = transparentColorTmp;
					//Set transparent color for drawing bmp | ���õ�ǰ��͸��ɫ���ڻ���BMP
					transparentColor[i] = RGB(transparentColorTmp.r,
						transparentColorTmp.g, transparentColorTmp.b);
				}
				else
				{
					transparentColor[i] = transparentColorOrginal;
				}
			}
		}

		memset(bmi->bmiColors, 255, 256 * sizeof(RGBQUAD));
		//��ɫ��
		//In fact, palette_color_count+1 is the number of colors in the palette
		//But type of palette_color_count is unsigned char, whose max value is 255,
		// so 0 is use to represent 1 color, 2 is use to represent 2 colors, etc.
		for (int j = 0; j < colorTableColorCount; ++j)
		{
			bmi->bmiColors[j].rgbRed = colorTable[j].r;
			bmi->bmiColors[j].rgbGreen = colorTable[j].g;
			bmi->bmiColors[j].rgbBlue = colorTable[j].b;
			bmi->bmiColors[j].rgbReserved = 0;
		}

		//������ɫ��������
		unsigned char* theBitmapdata = NULL;
		int mod;
		//bmpҪ��ÿ��������ռ�õ��ڴ��СΪ4������������Ĳ�0
		bool pitchIsFit = (mod = imageWidth[i] % 4) == 0;
		if (!pitchIsFit)
		{
			int pitch = imageWidth[i] + (4 - mod);//bmp��һ�����ݵ��ֽ���(���㵽4��������)
			theBitmapdata = (unsigned char*)malloc(pitch*imageHeight[i]);
			memset(theBitmapdata, 0, pitch*imageHeight[i]);
			for (unsigned int j = 0; j < imageHeight[i]; ++j)
			{
				memcpy(theBitmapdata + j*pitch, pColorIndexArray + j*imageWidth[i], imageWidth[i]);
			}
		}
		else
		{
			theBitmapdata = pColorIndexArray;
		}


		//����bmp
		bitmap = CreateDIBitmap(GetDC(hWnd), &bmi->bmiHeader, CBM_INIT, theBitmapdata, bmi, DIB_RGB_COLORS);
		if (NULL == bitmap)
		{
			MessageBox(NULL, _T("CreateDIBitmapʧ�ܣ�"), _T(APP_TITLE), NULL);
			return 1;
		}
		if (!pitchIsFit)
		{
			free(theBitmapdata);
		}
		free(bmi);
		delete[] colorTable;
	}

	for (unsigned int i = 0; i < frameCount; i++)
	{
		free(ppColorIndexArray[i]);
	}
	free(ppColorIndexArray);
	free(globalPalette);

	hdcsource = CreateCompatibleDC(GetDC(hWnd));
	if (NULL == hdcsource)
	{
		MessageBox(NULL, _T("CreateCompatibleDCʧ�ܣ�"), _T(APP_TITLE), NULL);
		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	ULONGLONG lastTimeInMS = 0;
	ULONGLONG currentTimeInMS = 0;
	ULONGLONG lastIntervalTimeInMS = 0;
	ULONGLONG timeSinceLastFrame = 0;
	BOOL MsgSent = FALSE;
	// Main message loop:
	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		//����Windows��Ϣ
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
				currentTimeInMS = GetTickCount64();
				timeSinceLastFrame += (currentTimeInMS - lastTimeInMS);
				//char buf[100];
				//sprintf(buf, "%d\n", timeSinceLastFrame);
				//OutputDebugStringA(buf);
				if (timeSinceLastFrame >= frameTimeInMS[currentFrame])//�ж��Ƿ�Ҫ������һ֡ͼ��
				{
					//ѭ������ͼ��
					if (currentFrame == frameCount-1)
					{
						currentFrame = 0;
					}
					else
					{
						++currentFrame;
					}
					//����
					timeSinceLastFrame = 0;
					//force�ػ�����
					InvalidateRect(hWnd, 0, FALSE);
					//OutputDebugStringA("invalidated\n");
				}
				lastTimeInMS = currentTimeInMS;
		}
	}

	free(*palette);
	free(palette);

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
			HGDIOBJ original = SelectObject(hdcsource, vBitmap[currentFrame]);
			if (NULL == original || HGDI_ERROR == original)
			{
				MessageBox(NULL, _T("SelectObjectʧ�ܣ�"), _T(APP_TITLE), NULL);
				return 1;
			}

			BITMAP bm;
			::GetObject(vBitmap[currentFrame], sizeof(bm), &bm);
			if (currentFrame == 0)//���ڵ�һ֡���ƻ���
			{
				//���ƻ���
				//Draw a background soild rect for the canvas | ��һ�����ο򱳾���ʾ����
				if (0 == FillRect(hdc, &canvasRect, backgroundBrush))
				{
					OutputDebugString(_T("FillRectʧ�ܣ�\n"));
					return E_FAIL;
				}
			}

			//����Dispose Method
			//0:û��Ҫ��
			//1:��Ҫ��������ԭ��
			//2:�����ǰͼ������Ϊ����ɫ
			//3:�����ǰͼ�����򲢻�ԭ��Ⱦ��ͼ��ǰ��ͼ��
			//(win32������3��1��Ч���Ͽ���һ���ģ���Ϊ����Ⱦ��ͼ��Ĭ�ϻᱻ����)
			if (disposalMethod[currentFrame] == 2)
			{
				RECT imageRECT = {
					image_position_x[currentFrame], image_position_y[currentFrame],
					image_position_x[currentFrame] + imageWidth[currentFrame],
					image_position_y[currentFrame] + imageHeight[currentFrame]
				};
				if (0 == FillRect(hdc, &imageRECT, backgroundBrush))
				{
					OutputDebugString(_T("FillRectʧ�ܣ�\n"));
					return E_FAIL;
				}
			}

			//Draw the image | ����ͼ��
			if (!(flag[currentFrame] & 0x01))//No transparent color | û��͸��ɫ
			{
				if (NULL == BitBlt(hdc,
					canvasRect.left + image_position_x[currentFrame],
					canvasRect.top + image_position_y[currentFrame],
					bm.bmWidth, bm.bmHeight,
					hdcsource, 0, 0, SRCCOPY))
				{
					OutputDebugString(_T("BitBltʧ�ܣ�\n"));
					return E_FAIL;
				}
			}
			else
			{
				if (FALSE == TransparentBlt(hdc,
					canvasRect.left + image_position_x[currentFrame],
					canvasRect.top + image_position_y[currentFrame],
					bm.bmWidth, bm.bmHeight,
					hdcsource, 0, 0,
					bm.bmWidth, bm.bmHeight,
					transparentColor[currentFrame]))
				{
					MessageBox(NULL, _T("TransparentBltʧ�ܣ�"), _T(APP_TITLE), NULL);
					return 1;
				}
			}

			SelectObject(hdc, original);
		}
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		DeleteDC(hdcsource);
		for (unsigned int i = 0; i < frameCount; i++)
		{
			DeleteObject(vBitmap[i]);
		}
		DeleteObject(backgroundBrush);

		delete[] disposalMethod;
		delete[] transparentColor;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}