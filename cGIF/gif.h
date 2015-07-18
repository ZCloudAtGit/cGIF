/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#pragma once
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#define MAX_CODE_CONTENT_LENGHTH 512 //Max length of the content of a code
//TODO
//去除这个假设！
//否则如果有code的的Content的长度超过MAX_CODE_CONTENT_LENGHTH时，解析出错

/*
cGIF错误定义
*/
typedef enum _error
{
	cGif_Success,
	cGif_File_Not_Exist,
	cGif_GCT_Not_Exist,
	cGif_Unknown_Introducer_Or_Label,
	//cGif_Next_Not_Clear_Or_EOI,//Deferred
	cGif_Encounter_EOI_Before_ImageData_Completed,
	cGif_Too_Many_Frames
} cGif_Error;

typedef struct _Flag
{
	unsigned char backeground : 1;
	unsigned char transparent : 1;
} cGif_Flag;

#pragma region 基础结构定义
/* the header */
struct _Header
{
	char Signature[3];
	char Version[3];
};
typedef struct _Header Header;

struct _LogicalScreenDescriptor
{
	/* Logical Screen Descriptor */
	unsigned short LogicalScreenWidth;
	unsigned short LogicalScreenHeight;
	unsigned int GCTFlag : 1;	//GCT Flag
	unsigned int ColorResolution : 3;
	unsigned int SortFlag : 1;
	unsigned int SizeOfGCT : 3;
	unsigned char BackgroundColorIndex;
	unsigned char PixelAspectRatio;
};
typedef struct _LogicalScreenDescriptor LogicalScreenDescriptor;

/* Color
	用于Global color table
		Local color table
*/
struct _Color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};
typedef struct _Color Color;

/* Graphics Control Extension */
struct _GraphicsControlExtension
{
	unsigned char Introducer;
	unsigned char Label;
	unsigned char BlockSize;
	int Reserved : 3;
	int DisposalMethod : 3;
	unsigned int UserInputFlag : 1;
	unsigned int TransparentColorFlag : 1;
	unsigned short DelayTime;
	unsigned char TransparentColorIndex;
	unsigned char BlockTerminator;
};
typedef struct _GraphicsControlExtension GraphicsControlExtension;

/*
* Image Data
*/

struct _ColorTable
{
    unsigned int ColorCount;
    Color* vColor;
};
typedef struct _ColorTable ColorTable;

/* Image Descriptor */
struct _ImageDescriptor
{
	unsigned char Separator;		//常数0x2C
	unsigned short LeftPosition;	//图像左边沿的位置
	unsigned short TopPosition;		//图像上边沿的位置
	unsigned short Width;			//图像宽度
	unsigned short Height;			//图像高度
	unsigned int LocalColorTableFlag : 1;	//Local color table是否可用
	unsigned int InterlaceFlag : 1;			//图像是否是交错的
	unsigned int SortFlag : 1;				//Local color table是否是有序的
	unsigned int Reserved : 2;				//保留位
	unsigned int SizeOfLocalColorTable : 3;	//Local color table的大小
};
typedef struct _ImageDescriptor ImageDescriptor;

struct _ApplicationExtension
{
    unsigned char Introducer;
    unsigned char Label;
    unsigned char BlockSize;
    unsigned char Identifier[8];
    unsigned char AuthCode[3];
    unsigned int SubBlockCount;//<=256
    unsigned char* SubBlock[256];
    unsigned char SubBlockSize[256];
	unsigned char BlockTerminator;
};
typedef struct _ApplicationExtension ApplicationExtension;

struct _CommentExtension
{
    unsigned char Introducer;
    unsigned char Label;
    unsigned int SubBlockCount;//<=256
    unsigned char* SubBlock[256];
    unsigned char SubBlockSize[256];
    unsigned char BlockTerminator;
};
typedef struct _CommentExtension CommentExtension;

struct _ImageData
{
    unsigned char LZWMinCodeSize;
    unsigned int SubBlockCount;//<=4096
    unsigned char** SubBlock;
    unsigned char* SubBlockSize;
    unsigned char BlockTerminator;
};
typedef struct _ImageData ImageData;

#pragma endregion

typedef struct _cGif_State_Global
{
	LogicalScreenDescriptor lsd;		//逻辑屏幕描述符
	ColorTable gct;						//全局Color table
	ApplicationExtension ae;			//应用程序控制扩展(现在的图像中仅存在“NETSCAPE2.0”这种扩展)
	cGif_Error error;					//错误
} cGif_State_Global;

typedef struct _cGif_State_Frame
{
	ColorTable lct;						//本地Color table
	GraphicsControlExtension gce;		//图像控制扩展
	ImageDescriptor imgDesc;			//图像描述符
} cGif_State_Frame;

#pragma region 基础结构操作函数声明

#pragma region SubBlock操作函数声明
//Release allocated memory for Sub Data
void ReleaseSubBlock(ImageData* pImgData, ApplicationExtension* pAppExt, CommentExtension* pCE);

//将ImageData中的Subblock数据整合为一整块
void CombineSubBlocks(ImageData* pImgData, unsigned char** pCodeData, unsigned int* pCodeDataByteSize);
#pragma endregion

#pragma region 解压缩相关函数声明
//从code数据块中提取code序列
cGif_Error ExtractCodeArray(unsigned char* CodeData, unsigned int CodeDataByteSize,
	unsigned int LZWMinCodeSize,
	unsigned int** ppCodeArray, unsigned int* pCodeArrayCount);

//将code序列翻译为颜色索引
cGif_Error TranslateCodeArray(unsigned char** ppColorIndexArray,
	ImageData* pImgData, ImageDescriptor* pImgDesc, unsigned int* CodeArray, unsigned int CodeArrayCount);

//对图像数据进行解压缩
cGif_Error DecompressImageData(unsigned char** out,
	ImageData* pImgData, ImageDescriptor* pImgDesc);
#pragma endregion

#pragma region 基础结构读取函数声明
//Read Header
void ReadHeader(Header* pHeader, FILE* file);

//Read Logical Screen Descriptor
void ReadLCD(LogicalScreenDescriptor* pLSD, FILE* file);

//Read Glocal Color Table
void ReadGCT(LogicalScreenDescriptor* pLSD, ColorTable* pGCT, FILE* file);

//Read Graphics Control Extension
void ReadGCE(GraphicsControlExtension* pGCE, FILE* file);

//Read Image Descriptor
void ReadImageDesc(ImageDescriptor* pImgDesc, FILE* file);

//Read Local Color Table
void ReadLCT(ImageDescriptor* pImgDesc, ColorTable* pLCT, FILE* file);

//Read Plain Text Extension
void ReadPTE(FILE* file);

void ReadImageData(ImageData* pImgData, FILE* file);

//Read Graphic Rendering Block
void ReadGRB(ImageData* pImgData, cGif_State_Frame* state, FILE* file);

//Read Application Extension
void ReadAE(ApplicationExtension* pAppExt, FILE* file);

//Read Comment Extension
void ReadCE(CommentExtension* pCommentExt, FILE* file);
//Skip Comment Extension
void SkipCE(FILE* file);
#pragma endregion

#pragma endregion

#pragma region 基础结构打印函数声明
void HeaderToString(Header* pHeader, char* buffer);
void LSDToString(LogicalScreenDescriptor* pLSD, char* buffer);
void ImageDescriptorToString(ImageDescriptor* pDesc, char* buffer);
void GCEToString(GraphicsControlExtension* pGCE, char* buffer);
void AEToString(ApplicationExtension* pAE, char* buffer);
void CEToString(CommentExtension* pCE, char* stringBuffer);
void ImageDataToString(ImageData* pImgData, char* stringBuffer);
void GCTToString(const ColorTable* GCT, char* stringBuffer );
void LCTToString(const ColorTable* LCT, char* stringBuffer);
#pragma endregion

#pragma region Code table定义及相关函数声明
//Code table定义
//value
//  code #i, i就是code的value
//content
//  二维数组
//      content[i]表示code #i的content
//      content[i][j]表示code #i的content的高位起第j个数
//length
//  一维数组
//      length[i]表示code #i的content长度
//count
//  表示code table中的code数量
//
//如第7个code：(code #7, content 112, length 3)
//  在CodeTable中的表示就是
//      content[7] = {1,1,2}; length[7] = 3;
//  或者，同样的：
//      content[7][0] = 1;
//      content[7][1] = 1;
//      content[7][2] = 2;
//      length[7] = 3;
//
//value的范围为[2^(2-1),2^(12-1)]即[2, 4096]
//      附加上Clear Code 和 EOI Code就是[4, 4098]
//content(颜色索引序列)的长度的范围为[1,INT_MAX]
//content中数字(颜色索引值)的范围为[0,255]
struct _CodeTable
{
	unsigned char** content;//4098, 2 << (12-1) + 2;//4096 + 2
	unsigned* length;
    unsigned int count;
};
typedef struct _CodeTable CodeTable;

void InitCodeTable(CodeTable* pCodeTable, unsigned short mincodeSize);
unsigned int AppendCode(CodeTable* pCodeTable,
    const unsigned char* content, const int length);
int CodeInTable(CodeTable* pCodeTable, unsigned int code);
void GetCodeContent(CodeTable* pCodeTable, unsigned int code, unsigned char** pContent, unsigned int *pLength);
void CodeTableToString(const CodeTable* pCodeTable, char* stringBuffer);
unsigned int GetCode( unsigned char* CodeData, unsigned int bitOffset,
    unsigned int codeLength );
void FreeCodeTable(CodeTable* pCodeTable, unsigned int InitialColorTableLength);
void ResetCodeTable(CodeTable* pCodeTable, unsigned short colorTableLength);
#pragma endregion