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

//用于颜色表（RGB顺序，注：GDI的RGBTRIPLE为BGR顺序）
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

//绘图相关变量
HPALETTE hpal = 0;//调色板句柄
HBITMAP bitmap = 0;//图象句柄
HDC hdcsource = 0;
BITMAPINFO* bmi = nullptr;
RECT canvasRect = { 0 };
UINT backgroundColor;
UINT transparentColor;
HBRUSH backgroundBrush = nullptr;

//cGIF参数
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
		MessageBox(NULL, _T("RegisterClassEx失败！"), _T(APP_TITLE), NULL);
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
		MessageBox(NULL, _T("CreateWindow失败！"), _T(APP_TITLE), NULL);
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
		sprintf(buffer, "解析GIF图像失败！错误：%s", cGif_error_text(error));
		MessageBoxA(NULL, buffer, APP_TITLE, NULL);
		return 1;
	}
	canvasRect.left = 150;
	canvasRect.top = 150;
	canvasRect.right = canvasRect.left + canvasWidth;
	canvasRect.bottom = canvasRect.top + canvasHeight;
	
	//处理索引数组
	//一行图像数据的大小必须是4字节的倍数，不足的要用0补齐
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
	//The color index data have been copied, so release it. | 颜色表数据已经复制过了，现在没用了，释放它
	free(pColorIndexArray);

	//处理背景色
	if (flag & 0x01)
	{
		backgroundColor = RGB(palette[background_color_index].r,
			palette[background_color_index].g, palette[background_color_index].b);
	}
	else
	{
		backgroundColor = RGB(0xff,0xff,0xff);//Default background color is white
	}

	//处理透明色
	if (flag & 0x02)
	{
		UINT transparentColorOrginal = RGB(palette[transparent_color_index].r,
			palette[transparent_color_index].g, palette[transparent_color_index].b);
		/*
		注意：
		GIF的颜色表中，透明色可能被设置为已有的普通颜色，比如颜色表中第3个颜色是黑色，第178个颜色也是黑色，
		但是178被设置为了透明色的索引。BMP无法按照索引的方式实现透明色，所以需要替换第178个颜色为颜色表
		中没有出现过的颜色，然后在TransparentBlt中指定该颜色为透明色，进行渲染。
		注意：渲染动态图像时，不能这么做，因为之前的帧所做的改变会影响后面的帧！
		*/
		RGBTriple& transparentColorTmp = palette[transparent_color_index];
		{
			bool bTransparentColorAlreadyUsed = false;
			//遍历GIF颜色表，判断透明色是否是已有的普通颜色
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
			//确定透明色是已有的普通颜色的话，找一个颜色表中没有的颜色替代它
			if (bTransparentColorAlreadyUsed == true)
			{
				//这里用随机颜色，因为256色在0xffffff色中还是很少的
				srand((int)palette);//随便找个Runtime值当种子，免得用time()还得加库
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
				//Replace original transparent color | 替换原来的透明色
				palette[transparent_color_index] = transparentColorTmp;
				//Set transparent color for drawing bmp | 设置当前的透明色用于绘制BMP
				transparentColor = RGB(transparentColorTmp.r,
					transparentColorTmp.g, transparentColorTmp.b);
			}
			else
			{
				transparentColor = transparentColorOrginal;
			}
		}
	}

	//获取disposal method
	int disposalMethod = ((flag & 0x1C) >> 2);

#if 1
	//创建背景色笔刷
	backgroundBrush = CreateSolidBrush(backgroundColor);


	//创建背景笔刷
#else
	//对于静态图像，
	//如果disposal method是2或3，
	//	如果有透明色，则使用透明色作为canvas背景，否则使用背景色；
	//如果disposal method不是2或3
	//  不用管canvas背景，也就是说不用绘制canvas背景
	if (disposalMethod == 2 || disposalMethod == 3)
	{
		if (flag & 0x01)//如果有透明色
			backgroundBrush = wcex.hbrBackground;
		else
			backgroundBrush = CreateSolidBrush(backgroundColor);
	}
#endif
	//创建、设置Bitmap信息
	bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD));//共256色，和bmpInfoHeader.biBitCount所指相同
	//头信息
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);   //BITMAPINFOHEADER的大小
	bmi->bmiHeader.biWidth = imageWidth;				//图像宽度（以像素为单位）
	bmi->bmiHeader.biHeight = -int(imageHeight);		//图像高度（以像素为单位），注意！此处取负值，因为位图默认第一行是图像最下面一行，而cGif_decode得到的数据的第一行是图像最上面一行
	bmi->bmiHeader.biPlanes = 1;						//必须被设置为1
	bmi->bmiHeader.biBitCount = 8;						//统一为256色图
	bmi->bmiHeader.biCompression = BI_RGB;				//不压缩的一般bmp格式
	bmi->bmiHeader.biSizeImage = 0;						//biCompression为BI_RGB时指定该值为0
	//无需考虑的值
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;
	//颜色表
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

	//创建bmp
	hdcsource = CreateCompatibleDC(NULL);
	if (NULL == hdcsource)
	{
		MessageBox(NULL, _T("CreateCompatibleDC失败！"), _T(APP_TITLE), NULL);
		return 1;
	}
	bitmap = CreateDIBitmap(GetDC(hWnd), &bmi->bmiHeader, CBM_INIT, vColorIndex, bmi, DIB_RGB_COLORS);
	free(vColorIndex);
	if (NULL == bitmap)
	{
		MessageBox(NULL, _T("CreateDIBitmap失败！"), _T(APP_TITLE), NULL);
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
				MessageBox(NULL, _T("SelectObject失败！"), _T(APP_TITLE), NULL);
				return 1;
			}

			//绘制画布
			//Draw a background soild rect for the canvas | 画一个矩形框背景表示画布
			if (0 == FillRect(hdc, &canvasRect, backgroundBrush))
			{
				OutputDebugString(_T("FillRect失败！\n"));
				return E_FAIL;
			}

			//Draw the image | 绘制图像
			if (! (flag & 0x02))//No transparent color | 没有透明色
			{
				if (NULL == BitBlt(hdc,
					canvasRect.left + image_position_x, canvasRect.top + image_position_y,
					imageWidth, imageHeight,
					hdcsource, 0, 0, SRCCOPY))
				{
					OutputDebugString(_T("BitBlt失败！\n"));
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
					OutputDebugString(_T("TransparentBlt失败！\n"));
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