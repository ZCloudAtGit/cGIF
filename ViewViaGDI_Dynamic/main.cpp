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

//用于颜色表（RGB顺序，注：GDI的RGBTRIPLE为BGR顺序）
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

//位图相关资源
HPALETTE hpal = 0;//调色板句柄
HBITMAP vBitmap[1024] = { 0 };//图象句柄
HDC hdcsource = 0;
RECT canvasRect = { 0 };
HBRUSH backgroundBrush = nullptr;

//cGIF参数
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

//动画控制相关
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
		sprintf(buffer, "解析GIF图像失败！错误：%s", cGif_error_text(error));
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
	动画的渲染：
	1.读取GIF文件，得到每一帧的图像数据
	2.按照顺序渲染每一帧图像，渲染时遵循Disposal 方式
	*/

	//Canvas信息
	//宽canvasWidth，高canvasHeight，
	//全局颜色表globalPalette，全局颜色表颜色数量globalPalette_color_count
	//背景颜色在全局颜色表中的索引background_color_index
	//图像帧数frameCount

	//处理背景色，背景色指画布(canvas)的颜色
	if (globalFlag & 0x01)
	{
		backgroundColor = RGB(globalPalette[background_color_index].r,
			globalPalette[background_color_index].g, globalPalette[background_color_index].b);
	}
	else
	{
		backgroundColor = RGB(0xff, 0xff, 0xff);//Default background color is white
	}
	//创建背景色笔刷
	backgroundBrush = CreateSolidBrush(backgroundColor);

	//对于有Global Color Table的文件，GCT提供给那些没有Local Color Table的图像帧使用；
	//如果图像帧已有LCT，则采用自己的LCT渲染
	//如果文件中没有GCT，而图像帧也没有LCT，则报错
	bool HasGCT = (globalPalette != NULL);

	//每帧创建1个bitmap对象
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

		//Disposal 方式
		disposalMethod[i] = ((flag[i] & 0x1C) >> 2);
		
		/*
		注意：无论GIF图像是多少位的，之后均按照8位进行渲染
		*/

		//创建、设置Bitmap信息
		BITMAPINFO* bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD));
		//头信息
		bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);   //BITMAPINFOHEADER的大小
		bmi->bmiHeader.biWidth = imageWidth[i];				//图像宽度（以像素为单位）
		bmi->bmiHeader.biHeight = -int(imageHeight[i]);		//图像高度（以像素为单位），注意！此处取负值，因为位图默认第一行是图像最下面一行，而cGif_decode得到的数据的第一行是图像最上面一行
		bmi->bmiHeader.biPlanes = 1;						//必须被设置为1
		bmi->bmiHeader.biBitCount = 8;						//均按照8位渲染
		bmi->bmiHeader.biCompression = BI_RGB;				//不压缩的一般bmp格式
		bmi->bmiHeader.biSizeImage = 0;						//biCompression为BI_RGB时指定该值为0
		//无需考虑的值
		bmi->bmiHeader.biXPelsPerMeter = 0;
		bmi->bmiHeader.biYPelsPerMeter = 0;
		bmi->bmiHeader.biClrUsed = 0;
		bmi->bmiHeader.biClrImportant = 0;

		//透明色
		if (flag[i] & 0x01)
		{
			/*
			注意：
				GIF的颜色表中，透明色可能被设置为已有的普通颜色，比如颜色表中第3个颜色是黑色，第178个颜色也是黑色，
				但是178被设置为了透明色的索引。BMP无法按照索引的方式实现透明色，所以需要替换第178个颜色为颜色表
				中没有出现过的颜色，然后在TransparentBlt中指定该颜色为透明色，进行渲染。
				*/
			UINT transparentColorOrginal = RGB(colorTable[transparent_color_index[i]].r,
				colorTable[transparent_color_index[i]].g, colorTable[transparent_color_index[i]].b);
			RGBTriple& transparentColorTmp = colorTable[transparent_color_index[i]];
			{
				bool bTransparentColorAlreadyUsed = false;
				//遍历GIF颜色表，判断透明色是否是已有的普通颜色
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
				//确定透明色是已有的普通颜色的话，找一个颜色表中没有的颜色替代它
				if (bTransparentColorAlreadyUsed == true)
				{
					//这里用随机颜色，因为256色在0xffffff色中还是很少的
					srand((int)colorTable);//随便找个Runtime值当种子，免得用time()还得加库
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
					//Replace original transparent color | 替换原来的透明色
					colorTable[transparent_color_index[i]] = transparentColorTmp;
					//Set transparent color for drawing bmp | 设置当前的透明色用于绘制BMP
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
		//颜色表
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

		//设置颜色索引数据
		unsigned char* theBitmapdata = NULL;
		int mod;
		//bmp要求每行索引所占用的内存大小为4的整数，不足的补0
		bool pitchIsFit = (mod = imageWidth[i] % 4) == 0;
		if (!pitchIsFit)
		{
			int pitch = imageWidth[i] + (4 - mod);//bmp中一行数据的字节数(补足到4的整数倍)
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


		//创建bmp
		bitmap = CreateDIBitmap(GetDC(hWnd), &bmi->bmiHeader, CBM_INIT, theBitmapdata, bmi, DIB_RGB_COLORS);
		if (NULL == bitmap)
		{
			MessageBox(NULL, _T("CreateDIBitmap失败！"), _T(APP_TITLE), NULL);
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
		MessageBox(NULL, _T("CreateCompatibleDC失败！"), _T(APP_TITLE), NULL);
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
		//处理Windows消息
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
				if (timeSinceLastFrame >= frameTimeInMS[currentFrame])//判断是否要绘制下一帧图像
				{
					//循环绘制图像
					if (currentFrame == frameCount-1)
					{
						currentFrame = 0;
					}
					else
					{
						++currentFrame;
					}
					//重置
					timeSinceLastFrame = 0;
					//force重画窗口
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
				MessageBox(NULL, _T("SelectObject失败！"), _T(APP_TITLE), NULL);
				return 1;
			}

			BITMAP bm;
			::GetObject(vBitmap[currentFrame], sizeof(bm), &bm);
			if (currentFrame == 0)//仅在第一帧绘制画布
			{
				//绘制画布
				//Draw a background soild rect for the canvas | 画一个矩形框背景表示画布
				if (0 == FillRect(hdc, &canvasRect, backgroundBrush))
				{
					OutputDebugString(_T("FillRect失败！\n"));
					return E_FAIL;
				}
			}

			//处理Dispose Method
			//0:没有要求
			//1:不要处理，保持原样
			//2:清除当前图像区域为背景色
			//3:清除当前图像区域并还原渲染本图像前的图像
			//(win32程序中3和1从效果上看是一样的，因为被渲染的图像默认会被保留)
			if (disposalMethod[currentFrame] == 2)
			{
				RECT imageRECT = {
					image_position_x[currentFrame], image_position_y[currentFrame],
					image_position_x[currentFrame] + imageWidth[currentFrame],
					image_position_y[currentFrame] + imageHeight[currentFrame]
				};
				if (0 == FillRect(hdc, &imageRECT, backgroundBrush))
				{
					OutputDebugString(_T("FillRect失败！\n"));
					return E_FAIL;
				}
			}

			//Draw the image | 绘制图像
			if (!(flag[currentFrame] & 0x01))//No transparent color | 没有透明色
			{
				if (NULL == BitBlt(hdc,
					canvasRect.left + image_position_x[currentFrame],
					canvasRect.top + image_position_y[currentFrame],
					bm.bmWidth, bm.bmHeight,
					hdcsource, 0, 0, SRCCOPY))
				{
					OutputDebugString(_T("BitBlt失败！\n"));
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
					MessageBox(NULL, _T("TransparentBlt失败！"), _T(APP_TITLE), NULL);
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