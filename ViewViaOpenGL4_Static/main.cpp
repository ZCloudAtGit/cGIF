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

#include "GLEW/include/GL/glew.h"
#include <GL/GL.h>
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "glu32.lib")
#pragma comment (lib, "GLEW/lib/glew32.lib")

extern "C"
{
#include "../cGIF/cGIF.h"
}
#pragma comment(lib, "../Debug/cGIF.lib")

#define APP_TITLE "GIF Viewer via OpenGL 4 (Static)"

//用于颜色表（RGB顺序，注：GDI的RGBTRIPLE为BGR顺序）
typedef struct _tagRGBTriple
{
	BYTE r;
	BYTE g;
	BYTE b;
} RGBTriple;

// The main window class name.
static char szWindowClass[] = "ViewViaOpenGL4_StaticWindowClass";
// The string that appears in the application's title bar.
static char szTitle[] = APP_TITLE;
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

//OpenGL相关数据
HGLRC hglrc = 0;
char titleWithOpenGLInfo[512] = { 0 };
//texture
GLuint ct = 0, tex = 0;
GLuint ctBuf = 0, texBuf = 0;
//shader
GLuint vertexShader=0, fragmentShader=0;
GLuint theShaderProgram=0;
//vertex
GLuint vao = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void InitShader();
void LoadTexture();
void DrawPicture();

#define checkOGLError _checkOGLError(__FILE__, __LINE__)

void _checkOGLError(char *file, int line)
{
	GLenum e;
	char errbuf[1024];
	int    retCode = 0;
	e = glGetError();
	if (e != GL_NO_ERROR)
	{
		sprintf_s(errbuf, "%s(%d): Error %d: %s\n",
			file, line, e, gluErrorString(e));
		OutputDebugStringA(errbuf);
		exit(e);
	}
}

void LoadQuad(int width, int height);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEXA wcex;

	wcex.cbSize = sizeof(WNDCLASSEXA);
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

	if (!RegisterClassExA(&wcex))
	{
		MessageBox(NULL, _T("RegisterClassEx失败！"), _T(APP_TITLE), NULL);
		return 1;
	}

	HWND hWnd = CreateWindowA(
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

	hdc = GetDC(hWnd);//保存DC到全局变量中

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
	while (msg.message!=WM_QUIT)
	{
		//处理Windows消息
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			assert(hglrc != NULL);
			if (IsLoaded == FALSE)//判断是否已加载图像数据
			{
				RECT rc;
				::GetClientRect(hWnd, &rc);
				int rc_width = rc.right - rc.left;
				int rc_height = rc.bottom - rc.top;
				//设置OpenGL的一些状态
				//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glEnable(GL_TEXTURE_1D);
				glEnable(GL_TEXTURE_2D);
				glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
				glViewport(0.5*(rc_width - imageWidth), 0.5*(rc_width - imageHeight), imageWidth, imageHeight);
				LoadQuad(imageWidth, imageHeight);
				//将图像数据传送到OpenGL引擎
				LoadTexture();
				//初始化shader
				InitShader();
				IsLoaded = TRUE;
			}
			else//已加载图像数据，用OpenGL渲染图像
			{
				DrawPicture();
			}
		}
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		GLenum err;
		char errbuf[1024];
		PIXELFORMATDESCRIPTOR pfd =
		{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
			32,                        //Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                       //Number of bits for the depthbuffer
			8,                        //Number of bits for the stencilbuffer
			0,                        //Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};

		hdc = GetDC(hWnd);

		int  pixelFormatIndex;
		pixelFormatIndex = ChoosePixelFormat(hdc, &pfd);
		SetPixelFormat(hdc, pixelFormatIndex, &pfd);

		hglrc = wglCreateContext(hdc);
		wglMakeCurrent(hdc, hglrc);

		char *GL_version = (char *)glGetString(GL_VERSION);
		sprintf_s(titleWithOpenGLInfo, "%s | opengl version: %s", szTitle, GL_version);
		SetWindowTextA(hWnd, titleWithOpenGLInfo);
		err = glewInit();
		if (err != GLEW_NO_ERROR)
		{
			sprintf_s(errbuf, "glewInit 失败 Error: %s\n", glewGetErrorString(err));
			OutputDebugStringA(errbuf);
			exit(1);
		}
	}
	break;
	case WM_DESTROY:
		wglDeleteContext(hglrc);
		hglrc = 0;
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
	//创建颜色表1D Texture
	{
		glGenTextures(1, &ct);
		checkOGLError;

		glBindTexture(GL_TEXTURE_1D, ct);
		checkOGLError;
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		checkOGLError;

		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8UI,
			palette_color_count, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, palette);
		checkOGLError;

		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		checkOGLError;
		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		checkOGLError;

	}

	//创建图像索引2D Texture
	{
		glGenTextures(1, &tex);
		checkOGLError;

		glBindTexture(GL_TEXTURE_2D, tex);
		checkOGLError;

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		checkOGLError;

		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
			imageWidth, imageHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, pColorIndexArray);
		checkOGLError;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		checkOGLError;
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		checkOGLError;
	}

}

void InitShader()
{
	GLint compileErr, linkErr;
	char errbuf[1024];

	// Shader
	{
		//TODO create WVP matrix and multiply it with the vertex position
		static const char* vertexShaderString =
			"#version 410 core\n"
			"layout(location = 0) in vec2 in_position;"
			"layout(location = 1) in vec2 in_tex_coord; "
			"out vec2 tex_coord0;"
			"void main(void)"
			"{"
			"	gl_Position = vec4(in_position, 0.5f, 1.0f);"
			"	tex_coord0 = in_tex_coord;"
			"}";
		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		checkOGLError;

		glShaderSource(vertexShader, 1, &vertexShaderString, NULL);
		checkOGLError;

		glCompileShader(vertexShader);
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &compileErr);
		if (compileErr != GL_TRUE)
		{
			glGetShaderInfoLog(vertexShader, 1024, NULL, errbuf);
			OutputDebugStringA(errbuf);
			exit(1);
		}

		static const char* fragmentShaderString =
			"#version 410 core\n"
			"uniform float transparent_color_index;"
			"in vec2 tex_coord0;"
			"layout (location = 0) out vec4 color;"
			"uniform usampler1D colorTable;"
			"uniform usampler2D picture;"
			"void main(void)"
			"{"
			"	uint colorindex = texture(picture, tex_coord0).x;"
			"	color = texelFetch(colorTable, int(colorindex), 0)/256.0f;"
			"	if (colorindex == transparent_color_index)"
			"	{"
			"		discard;"
			"	}"
			"}";
		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		checkOGLError;

		glShaderSource(fragmentShader, 1, &fragmentShaderString, NULL);
		checkOGLError;

		glCompileShader(fragmentShader);
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &compileErr);
		if (compileErr != GL_TRUE)
		{
			glGetShaderInfoLog(fragmentShader, 1024, NULL, errbuf);
			OutputDebugStringA(errbuf);
			exit(1);
		}
	}

	// Program
	{
		theShaderProgram = glCreateProgram();
		checkOGLError;

		glAttachShader(theShaderProgram, vertexShader);
		checkOGLError;

		glAttachShader(theShaderProgram, fragmentShader);
		checkOGLError;

		glLinkProgram(theShaderProgram);
		glGetProgramiv(theShaderProgram, GL_LINK_STATUS, &linkErr);
		if (linkErr != GL_TRUE)
		{
			glGetProgramInfoLog(theShaderProgram, 1024, NULL, errbuf);
			OutputDebugStringA(errbuf);
			exit(1);
		}

		glUseProgram(theShaderProgram);
		checkOGLError;
	}
}

void LoadQuad(int width, int height)
{
	// a Quad to render the image
	static const GLfloat quad_data[] =
	{
		// Vertex positions
		-1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		// Texture coordinates
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};

	// Create and initialize a buffer object
	GLuint buf;
	glGenBuffers(1, &buf);
	checkOGLError;

	glBindBuffer(GL_ARRAY_BUFFER, buf);
	checkOGLError;

	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
	checkOGLError;

	// Setup vertex attributes
	glGenVertexArrays(1, &vao);
	checkOGLError;

	glBindVertexArray(vao);
	checkOGLError;

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
	checkOGLError;

	glEnableVertexAttribArray(0);
	checkOGLError;

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0,
		(GLvoid*)(16 * sizeof(float)));
	checkOGLError;

	glEnableVertexAttribArray(1);
	checkOGLError;
}

void DrawPicture()
{
	glClear(GL_COLOR_BUFFER_BIT);
	checkOGLError;

	//Set shader
	//color table
	GLint ctLocation = glGetUniformLocation(theShaderProgram, "colorTable");
	checkOGLError;
	if (ctLocation == -1)
	{
		OutputDebugStringA("Error: semantic <colorTable>不存在！\n");
		exit(1);
	}

	glActiveTexture(GL_TEXTURE0);
	checkOGLError;

	glBindTexture(GL_TEXTURE_1D, ct);
	checkOGLError;

	glUniform1i(ctLocation, 0);
	checkOGLError;

	//picture
	GLint pictureLocation = glGetUniformLocation(theShaderProgram, "picture");
	checkOGLError;
	if (pictureLocation == -1)
	{
		OutputDebugStringA("Error: semantic <picture>不存在！\n");
		exit(1);
	}

	//transparentColorIndex
	GLint transparentColorIndexLocation;
	transparentColorIndexLocation = glGetUniformLocation(theShaderProgram, "transparent_color_index");
	checkOGLError;
	if (transparentColorIndexLocation == -1)
	{
		OutputDebugStringA("Error: semantic <transparent_color_index>不存在！\n");
		exit(1);
	}


	glActiveTexture(GL_TEXTURE1);
	checkOGLError;

	glBindTexture(GL_TEXTURE_2D, tex);
	checkOGLError;

	glUniform1i(pictureLocation, 1);
	checkOGLError;

	glUniform1f(transparentColorIndexLocation, transparent_color_index);
	checkOGLError;



	glBindVertexArray(vao);
	checkOGLError;

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	checkOGLError;

	//draw
	glFlush();
	SwapBuffers(hdc);
}