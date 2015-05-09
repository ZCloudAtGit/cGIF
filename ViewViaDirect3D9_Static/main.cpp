/*
(c) 2014-2015 cloud
Website: http:\\cloud.s2.jutuo.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <tchar.h>

#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

extern "C"
{
#include "../cGIF/cGIF.h"
}
#pragma comment(lib, "../Debug/cGIF.lib")

#define APP_TITLE "GIF Viewer via Direct3D 9 (Static)"

//用于颜色表（RGB顺序，注：GDI的RGBTRIPLE为BGR顺序）
typedef struct _tagRGBTriple
{
	BYTE r;
	BYTE g;
	BYTE b;
} RGBTriple;

// Global variables
// The main window class name.
static TCHAR szWindowClass[] = _T("ViewViaDirect3D9_StaticWindowClass");
// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T(APP_TITLE);
HDC hdc = 0;

//GIF相关数据
//cGIF参数
unsigned char* pColorIndexArray = NULL;
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

//Direct3D相关数据
D3DVIEWPORT9 viewport;
IDirect3D9* g_pD3D = 0;
IDirect3DDevice9* g_pDevice = 0;
IDirect3DTexture9* g_pColorTable = 0, *g_pPicture = 0;
IDirect3DVertexDeclaration9* g_pVD = 0;
IDirect3DVertexBuffer9* g_pVB = 0;
//shader
IDirect3DVertexShader9* g_pVS = 0;
ID3DXConstantTable* g_pVSCT = 0;
IDirect3DPixelShader9* g_pPS = 0;
ID3DXConstantTable* g_pPSCT = 0;
//matrix
D3DXMATRIX matIdentity, matWorld, matView, matProj;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void InitShader();
void LoadTexture();
void DrawPicture();

#define checkD3D9Error(hr) _checkD3D9Error(hr, __FILE__, __LINE__)

void _checkD3D9Error(HRESULT hr, char *file, int line)
{
	char errbuf[1024];
	int    retCode = 0;
	if (hr != S_OK)
	{
		sprintf_s(errbuf, "%s(%d): Error %d\n",
			file, line, hr);
		OutputDebugStringA(errbuf);
		exit(hr);
	}
}

void LoadQuad(int width, int height);

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
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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

	hdc = GetDC(hWnd);//保存DC到全局变量中，因为后面频繁使用

	//读取GIF图像，取得图像数据
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

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Main message loop:
	MSG msg = { 0 };
	BOOL IsLoaded = FALSE;
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
			assert(g_pD3D != NULL);
			assert(g_pDevice != NULL);
			if (IsLoaded == FALSE)
			{
				RECT rc;
				::GetClientRect(hWnd, &rc);
				int rc_width = rc.right - rc.left;
				int rc_height = rc.bottom - rc.top;
				viewport.X = 0;
				viewport.Y = 0;
				viewport.Width = rc_width;
				viewport.Height = rc_height;
				viewport.MinZ = 0.0f;
				viewport.MaxZ = 1.0f;
				LoadQuad(imageWidth, imageHeight);
				//将图像数据传送到Direct3D9引擎
				LoadTexture();
				//初始化shader
				InitShader();
				IsLoaded = TRUE;
			}
			else//已创建，用D3D渲染图像
			{
				DrawPicture();
			}
		}
	}

	//注销window类
	UnregisterClass(szWindowClass, hInstance);

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{

		//初始化COM
		CoInitialize(NULL);
		//创建D3D9对象
		HRESULT hr = S_OK;
		g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
		if (g_pD3D==NULL)
		{
			OutputDebugStringA("创建D3D9对象失败！\n");
		}
		//创建D3D9设备
		D3DDISPLAYMODE d3ddm;
		hr = g_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
		checkD3D9Error(hr);

		hr = g_pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, D3DFMT_A8R8G8B8, TRUE);
		checkD3D9Error(hr);


		D3DPRESENT_PARAMETERS d3dpp;
		ZeroMemory(&d3dpp, sizeof(D3DPRESENT_PARAMETERS));
		d3dpp.Windowed = TRUE;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;

		hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING,
			&d3dpp, &g_pDevice);
		checkD3D9Error(hr);
	}
	break;
	case WM_DESTROY:
		//Release D3D9 Objects
		g_pVD->Release();
		g_pVB->Release();
		g_pVS->Release();
		g_pVSCT->Release();
		g_pPS->Release();
		g_pPSCT->Release();
		g_pDevice->Release();
		g_pD3D->Release();
		CoUninitialize();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
}

void LoadTexture()
{
	HRESULT hr = S_OK;
	D3DLOCKED_RECT rect;

	//Create texture and fill it with the palette
	{
		hr = g_pDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_pColorTable, NULL);
		checkD3D9Error(hr);
		hr = g_pColorTable->LockRect(0, &rect, 0, D3DLOCK_NOSYSLOCK);
		checkD3D9Error(hr);
		unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
		for (int i = 0; i <= palette_color_count; ++i)
		{
			dest[0 + i * 4] = palette[i].b;
			dest[1 + i * 4] = palette[i].g;
			dest[2 + i * 4] = palette[i].r;
			dest[3 + i * 4] = 255;
		}
		if (flag & 0x02)
		{
			//Set alpha of the transparent color as 0
			dest[3 + transparent_color_index * 4] = 0;
		}

		hr = g_pColorTable->UnlockRect(0);
		checkD3D9Error(hr);
	}

	//Create texture and fill it with the color indexes
	{
		hr = g_pDevice->CreateTexture(imageWidth, imageHeight, 1, 0, D3DFMT_A8, D3DPOOL_MANAGED, &g_pPicture, NULL);
		checkD3D9Error(hr);
		hr = g_pPicture->LockRect(0, &rect, 0, D3DLOCK_NOSYSLOCK);
		checkD3D9Error(hr);
		unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
		for (unsigned int y = 0; y < imageHeight; ++y)
		{
			memcpy(dest, pColorIndexArray + y*imageWidth, imageWidth);
			dest += rect.Pitch;
		}
		hr = g_pPicture->UnlockRect(0);
		checkD3D9Error(hr);
	}
}

void InitShader()
{
	HRESULT hr = S_OK;
	ID3DXBuffer* pBuffer = 0;
	ID3DXBuffer* pErrBuffer = 0;

	// Shader
	static const char* vertexShaderString = R"(
float4x4 matWorld;
float4x4 matView;
float4x4 matProj;

struct InputVS
{
	float3 Pos:		POSITION0;
	float2 TC:		TEXCOORD0;
};

struct OutputVS
{
	float4 Pos:		POSITION0;
	float2 TC:		TEXCOORD0;
};

OutputVS main(InputVS In)
{
	OutputVS outVS = (OutputVS)0;
	float4 pos4 = float4(In.Pos,1.0f);
	pos4 = mul(pos4, matWorld);
	pos4 = mul(pos4, matView);
	pos4 = mul(pos4, matProj);
	outVS.Pos = pos4;
	outVS.TC = In.TC;
	return outVS;
}
)";
	hr = D3DXCompileShader(vertexShaderString, strlen(vertexShaderString), 0, 0, "main", "vs_3_0", 0, &pBuffer, &pErrBuffer, &g_pVSCT);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)pErrBuffer->GetBufferPointer());
		exit(hr);
	}
	hr = g_pDevice->CreateVertexShader((DWORD*)pBuffer->GetBufferPointer(), &g_pVS);
	checkD3D9Error(hr);

	static char* pixelShaderString;
	if (flag & 0x01)
	{
		pixelShaderString =
			"float backgroundColorIndex;"
			"texture1D colorTable: register( s0 );"
			"texture2D picture : register( s1 );"
			"sampler1D colorTableSampler = sampler_state"
			"{"
			"	texture = <colorTable>;"
			"	MipFilter = NONE;"
			"	MinFilter = NONE;"
			"	MagFilter = NONE;"
			"	AddressU = Wrap;"
			"};"
			"sampler2D pictureSampler = sampler_state"
			"{"
			"	texture = <picture>;"
			"	MipFilter = LINEAR;"
			"	MinFilter = LINEAR;"
			"	MagFilter = LINEAR;"
			"};"
			"struct InputPS"
			"{"
			"	float2 TC: TEXCOORD0;"
			"};"
			"struct OutputPS"
			"{"
			"	float4 Color: COLOR0;"
			"};"
			"OutputPS main(InputPS In)"
			"{"
			"	OutputPS outPS;"
			"	float colorindex = tex2D(pictureSampler, In.TC).a;"
			//HACK to get last texel of colortable(at 1.0f or 255)
			//We can not get the right texel but get the first texel incorrectly when colorindex is 1.0f
			"	if (colorindex == 1.0f)"
			"	{"
			"		colorindex -= 1/255.0f;"
			"	}"
			"	outPS.Color = tex1D(colorTableSampler, colorindex);"
			"	if (outPS.Color.a == 0.0f)"//Fill transparent pixel with the background color
			"	{"
			"		outPS.Color = tex1D(colorTableSampler, backgroundColorIndex);"
			"	}"
			"	else"
			"	{"
			"		outPS.Color = tex1D(colorTableSampler, colorindex);"
			"	}"
			"	return outPS;"
			"}";
	}
	else
	{
		pixelShaderString =
			"texture1D colorTable: register( s0 );"
			"texture2D picture : register( s1 );"
			"sampler1D colorTableSampler = sampler_state"
			"{"
			"	texture = <colorTable>;"
			"	MipFilter = NONE;"
			"	MinFilter = NONE;"
			"	MagFilter = NONE;"
			"	AddressU = Wrap;"
			"};"
			"sampler2D pictureSampler = sampler_state"
			"{"
			"	texture = <picture>;"
			"	MipFilter = LINEAR;"
			"	MinFilter = LINEAR;"
			"	MagFilter = LINEAR;"
			"};"
			"struct InputPS"
			"{"
			"	float2 TC: TEXCOORD0;"
			"};"
			"struct OutputPS"
			"{"
			"	float4 Color: COLOR0;"
			"};"
			"OutputPS main(InputPS In)"
			"{"
			"	OutputPS outPS;"
			"	float colorindex = tex2D(pictureSampler, In.TC).a;"
			//HACK to get last texel of colortable(at 1.0f or 255)
			//We can not get the right texel but get the first texel incorrectly when colorindex is 1.0f
			"	if (colorindex == 1.0f)"
			"	{"
			"		colorindex -= 1/255.0f;"
			"	}"
			"	outPS.Color = tex1D(colorTableSampler, colorindex);"
			"	return outPS;"
			"}";
	}

	hr = D3DXCompileShader(pixelShaderString, strlen(pixelShaderString), 0, 0, "main", "ps_3_0", 0, &pBuffer, &pErrBuffer, &g_pPSCT);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)pErrBuffer->GetBufferPointer());
		exit(hr);
	}
	hr = g_pDevice->CreatePixelShader((DWORD*)pBuffer->GetBufferPointer(), &g_pPS);
	checkD3D9Error(hr);
}

void SetConstants()
{
	//Build matrixes
	D3DXMatrixIdentity(&matIdentity);
	matWorld = matIdentity;
	D3DXMatrixOrthoLH(&matProj, (float)viewport.Width, (float)viewport.Height, 0.0f, 1.0f);
	D3DXVECTOR3 eyePoint = { 0.0f, 0.0f, 1.0f };
	D3DXVECTOR3 lookAt = { 0.0f, 0.0f, 0.0f };
	D3DXVECTOR3 up = { 0.0f, -1.0f, 0.0f };
	D3DXMatrixLookAtLH(&matView, &eyePoint, &lookAt, &up);

	//Set constants
	//common matrixes
	D3DXHANDLE hMatWorld = g_pVSCT->GetConstantByName(NULL, "matWorld");
	D3DXHANDLE hMatView = g_pVSCT->GetConstantByName(NULL, "matView");
	D3DXHANDLE hMatProj = g_pVSCT->GetConstantByName(NULL, "matProj");
	HRESULT hr = g_pVSCT->SetMatrix(g_pDevice, hMatWorld, &matWorld);
	checkD3D9Error(hr);
	hr = g_pVSCT->SetMatrix(g_pDevice, hMatView, &matView);
	checkD3D9Error(hr);
	hr = g_pVSCT->SetMatrix(g_pDevice, hMatProj, &matProj);
	checkD3D9Error(hr);

	//background color index
	if (flag & 0x01)
	{
		D3DXHANDLE backgroundColorIndexConstHandle = g_pPSCT->GetConstantByName(NULL, "backgroundColorIndex");
		hr = g_pPSCT->SetFloat(g_pDevice, backgroundColorIndexConstHandle, background_color_index / 255.0f);
		checkD3D9Error(hr);
	}
}

void LoadQuad(int width, int height)
{
	//a quad for rendering the image | 一个长方形，图像会被绘制在它上面
	//The quad (0,0,width,height) is moved by (-0.5*width, -0.5*height)
	//so the center of it is at (0,0,0)
	//minus 0.5f to locate the pixel well
	//(see https://msdn.microsoft.com/en-us/library/windows/desktop/bb219690.aspx)
	static const float quad_data[] =
	{
		// Vertex positions										Texture coordinates
		-0.5f*width - 0.5f, -0.5f*height - 0.5f, 0.0f,			0.0f, 0.0f,
		0.5f*width - 0.5f, -0.5f*height - 0.5f, 0.0f,			1.0f, 0.0f,
		0.5f*width - 0.5f, 0.5f*height - 0.5f, 0.0f,			1.0f, 1.0f,
		-0.5f*width - 0.5f, 0.5f*(float)height - 0.5f, 0.0f,	0.0f, 1.0f,
	};

	g_pDevice->CreateVertexBuffer(sizeof(quad_data), 0, 0, D3DPOOL_MANAGED, &g_pVB, 0);
	void* data;
	g_pVB->Lock(0, sizeof(quad_data), &data, 0);
	memcpy(data, quad_data, sizeof(quad_data));
	g_pVB->Unlock();

	static const D3DVERTEXELEMENT9 Decl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	g_pDevice->CreateVertexDeclaration(Decl, &g_pVD);

	g_pDevice->SetVertexDeclaration(g_pVD);
}

void DrawPicture()
{
	HRESULT hr = S_OK;

	//清除整个客户区
	hr = g_pDevice->SetViewport(&viewport);
	checkD3D9Error(hr);
	hr = g_pDevice->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0);
	checkD3D9Error(hr);

	//如果使用透明色，通过Alpha Test去除透明像素
	if (flag & 0x02)
	{
		hr = g_pDevice->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x00000001);
		checkD3D9Error(hr);
		hr = g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		checkD3D9Error(hr);
		hr = g_pDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		checkD3D9Error(hr);
	}

	hr = g_pDevice->BeginScene();
	checkD3D9Error(hr);
	hr = g_pDevice->SetVertexShader(g_pVS);
	checkD3D9Error(hr);
	hr = g_pDevice->SetPixelShader(g_pPS);
	checkD3D9Error(hr);

	SetConstants();

	hr = g_pDevice->SetVertexDeclaration(g_pVD);
	checkD3D9Error(hr);
	hr = g_pDevice->SetStreamSource(0, g_pVB, 0, 20);
	checkD3D9Error(hr);
	hr = g_pDevice->SetTexture(0, g_pColorTable);
	checkD3D9Error(hr);
	hr = g_pDevice->SetTexture(1, g_pPicture);
	checkD3D9Error(hr);
	hr = g_pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
	checkD3D9Error(hr);
	hr = g_pDevice->EndScene();
	checkD3D9Error(hr);
	hr = g_pDevice->Present(0, 0, 0, 0);
	checkD3D9Error(hr);
}